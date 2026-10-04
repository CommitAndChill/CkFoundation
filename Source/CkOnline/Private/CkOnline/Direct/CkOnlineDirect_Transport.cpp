#include "CkOnline/Direct/CkOnlineDirect_Transport.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace ck::direct
{
namespace
{
    constexpr int32 MaxFrame = 4096;
    constexpr int32 MaxBuffered = 65536;
    constexpr int32 MaxPending = 64;
    constexpr int32 MaxPeers = 8;

    auto TryDeadline(double Now, double Duration, double& Deadline) -> bool
    {
        if (!FMath::IsFinite(Now) || Now < 0 || !FMath::IsFinite(Duration) || Duration <= 0) return false;
        const double Candidate = Now + Duration;
        if (!FMath::IsFinite(Candidate) || Candidate <= Now) return false;
        Deadline = Candidate;
        return true;
    }

    auto IsStrictUtf8(const uint8* Data, int32 Count) -> bool
    {
        for (int32 I = 0; I < Count;)
        {
            const uint8 A = Data[I++];
            if (A < 0x80) { if (A == 0) return false; continue; }
            int32 Extra = 0;
            uint32 Code = 0;
            if (A >= 0xC2 && A <= 0xDF) { Extra = 1; Code = A & 0x1F; }
            else if (A >= 0xE0 && A <= 0xEF) { Extra = 2; Code = A & 0x0F; }
            else if (A >= 0xF0 && A <= 0xF4) { Extra = 3; Code = A & 0x07; }
            else return false;
            if (I + Extra > Count) return false;
            for (int32 J = 0; J < Extra; ++J)
            {
                const uint8 B = Data[I++];
                if ((B & 0xC0) != 0x80) return false;
                Code = (Code << 6) | (B & 0x3F);
            }
            if ((Extra == 1 && Code < 0x80) || (Extra == 2 && Code < 0x800) ||
                (Extra == 3 && Code < 0x10000) || (Code >= 0xD800 && Code <= 0xDFFF) ||
                Code > 0x10FFFF) return false;
        }
        return true;
    }

    auto IsValidString(const FString& Value, int32 MaxCharacters, int32 MaxBytes) -> bool
    {
        if (Value.IsEmpty() || Value.Len() > MaxCharacters) return false;
        for (int32 I = 0; I < Value.Len(); ++I)
        {
            const uint32 C = static_cast<uint32>(Value[I]);
            if (C < 32 || C == 127) return false;
#if PLATFORM_TCHAR_IS_4_BYTES
            if (C > 0x10FFFF || (C >= 0xD800 && C <= 0xDFFF)) return false;
#else
            if (C >= 0xD800 && C <= 0xDBFF)
            {
                if (++I >= Value.Len() || Value[I] < 0xDC00 || Value[I] > 0xDFFF) return false;
            }
            else if (C >= 0xDC00 && C <= 0xDFFF) return false;
#endif
        }
        const FTCHARToUTF8 Utf8(*Value);
        return Utf8.Length() <= MaxBytes;
    }

    auto ParseIpv4(const FString& Text, FIPv4Address& Address) -> bool
    {
        if (!FIPv4Address::Parse(Text, Address)) return false;
        FString FirstOctet;
        FString Remainder;
        if (!Text.Split(TEXT("."), &FirstOctet, &Remainder)) return false;
        const int32 First = FCString::Atoi(*FirstOctet);
        return Address.ToString() == Text && First >= 1 && First <= 223 &&
            Address != FIPv4Address(255, 255, 255, 255);
    }

    auto ParseDescriptor(const FString& Text, FIPv4Address& Address, int32& Port,
        FGuid& Epoch, FGuid& Capability) -> bool
    {
        if (Text.Len() > 160) return false;
        TArray<FString> Parts;
        Text.ParseIntoArray(Parts, TEXT("|"), false);
        if (Parts.Num() != 4 || Parts[0] != TEXT("ckd:1")) return false;
        FString Host;
        FString PortText;
        if (!Parts[1].Split(TEXT(":"), &Host, &PortText) ||
            !ParseIpv4(Host, Address) || PortText.IsEmpty() || PortText.Len() > 5) return false;
        for (const TCHAR C : PortText) if (C < TEXT('0') || C > TEXT('9')) return false;
        Port = FCString::Atoi(*PortText);
        return Port >= 1 && Port <= 65535 && FString::FromInt(Port) == PortText &&
            FGuid::ParseExact(Parts[2], EGuidFormats::Digits, Epoch) && Epoch.IsValid() &&
            FGuid::ParseExact(Parts[3], EGuidFormats::Digits, Capability) && Capability.IsValid();
    }

    auto GuidText(const FGuid& Id) -> FString { return Id.ToString(EGuidFormats::Digits); }
    auto GuidField(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, FGuid& Out) -> bool
    {
        FString Value;
        return Json->TryGetStringField(Key, Value) &&
            FGuid::ParseExact(Value, EGuidFormats::Digits, Out) && Out.IsValid();
    }
    auto StringField(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, FString& Out) -> bool
    { return Json->TryGetStringField(Key, Out); }
    auto BoolField(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, bool& Out) -> bool
    { return Json->TryGetBoolField(Key, Out); }
    auto IntegerField(const TSharedPtr<FJsonObject>& Json, const TCHAR* Key, int32& Out) -> bool
    {
        double Number = 0;
        if (!Json->TryGetNumberField(Key, Number) || !FMath::IsFinite(Number) ||
            Number < 0 || Number > MAX_int32 || FMath::FloorToDouble(Number) != Number) return false;
        Out = static_cast<int32>(Number);
        return true;
    }
}

struct FTransport::FImpl
{
    struct FPeer
    {
        FSocket* Socket = nullptr;
        TArray<uint8> Receive;
        TArray<uint8> Send;
        int32 SendOffset = 0;
        FGuid MemberId;
        FString Name;
        bool Admitted = false;
        bool Connecting = false;
        double WindowStart = 0;
        int32 WindowRequests = 0;
        double AcceptedAt = 0;
        TArray<FGuid> Recent;
        ~FPeer() { if (Socket) { Socket->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); } }
    };
    struct FPending { FGuid Wire; FGuid Public; ECk_OnlineDirect_Action Action; double Deadline; };

    FCk_OnlineDirect_Spec Spec;
    FCk_OnlineDirect_Snapshot Snapshot;
    FSocket* Listener = nullptr;
    TArray<TUniquePtr<FPeer>> Peers;
    TArray<FPending> Pending;
    TArray<FTransportCompletion> Completions;
    FGuid Epoch;
    FGuid Capability;
    FGuid JoinWire;
    double Expiry = 0;
    double LastNow = 0;
    int32 Sequence = 0;
    bool Hosting = false;

    explicit FImpl(const FCk_OnlineDirect_Spec& InSpec) : Spec(InSpec)
    { Snapshot.Set_ServiceGeneration(FGuid::NewGuid()); }
    ~FImpl() { Close(ECk_OnlineDirect_Result::Cancelled); }

    auto Changed() -> void
    { if (Snapshot.Get_Revision() < MAX_int32) Snapshot.Set_Revision(Snapshot.Get_Revision() + 1); }
    auto Complete(FGuid Ticket, ECk_OnlineDirect_Result Result, const FString& Detail = {}) -> void
    { Completions.Add({Ticket, Result, Detail}); }
    auto CompleteWire(FGuid Wire, ECk_OnlineDirect_Result Result, const FString& Detail = {}) -> bool
    {
        for (int32 I = 0; I < Pending.Num(); ++I)
        {
            if (Pending[I].Wire != Wire) continue;
            Complete(Pending[I].Public, Result, Detail);
            Pending.RemoveAt(I);
            return true;
        }
        return false;
    }
    auto ClearPending(ECk_OnlineDirect_Result Result) -> void
    {
        for (const FPending& Item : Pending) Complete(Item.Public, Result);
        Pending.Reset();
    }
    auto Close(ECk_OnlineDirect_Result PendingResult) -> void
    {
        ClearPending(PendingResult);
        Peers.Reset();
        if (Listener)
        {
            Listener->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Listener);
            Listener = nullptr;
        }
        const bool WasOpen = Snapshot.Get_State() != ECk_OnlineDirect_State::Offline;
        Hosting = false;
        Epoch.Invalidate(); Capability.Invalidate(); JoinWire.Invalidate(); Expiry = 0; Sequence = 0;
        Snapshot.Set_State(ECk_OnlineDirect_State::Offline).Set_LobbyEpoch(FGuid())
            .Set_LocalMemberId(FGuid()).Set_JoinDescriptor(TEXT(""))
            .Set_LastError(TEXT(""));
        Snapshot.Get_Members().Reset(); Snapshot.Get_Messages().Reset();
        if (WasOpen) Changed();
    }
    auto FailClient(const FString& Reason) -> void
    {
        ClearPending(ECk_OnlineDirect_Result::TransportError);
        Peers.Reset();
        JoinWire.Invalidate();
        const bool Different = Snapshot.Get_State() != ECk_OnlineDirect_State::Failed ||
            Snapshot.Get_LastError() != Reason || !Snapshot.Get_Members().IsEmpty() ||
            !Snapshot.Get_Messages().IsEmpty() || Snapshot.Get_LobbyEpoch().IsValid();
        Snapshot.Set_State(ECk_OnlineDirect_State::Failed).Set_LastError(Reason)
            .Set_LobbyEpoch(FGuid()).Set_LocalMemberId(FGuid()).Set_JoinDescriptor(TEXT(""));
        Snapshot.Get_Members().Reset(); Snapshot.Get_Messages().Reset();
        if (Different) Changed();
    }
    auto Queue(FPeer& Peer, const TSharedRef<FJsonObject>& Json) -> bool
    {
        FString Body;
        const auto Writer = TJsonWriterFactory<>::Create(&Body);
        if (!FJsonSerializer::Serialize(Json, Writer)) return false;
        const FTCHARToUTF8 Utf8(*Body);
        const int32 Length = Utf8.Length();
        if (Length < 1 || Length > MaxFrame || Peer.Send.Num() - Peer.SendOffset + Length + 4 > MaxBuffered) return false;
        if (Peer.SendOffset > 0)
        {
            Peer.Send.RemoveAt(0, Peer.SendOffset, EAllowShrinking::No);
            Peer.SendOffset = 0;
        }
        Peer.Send.Add(static_cast<uint8>((Length >> 24) & 255));
        Peer.Send.Add(static_cast<uint8>((Length >> 16) & 255));
        Peer.Send.Add(static_cast<uint8>((Length >> 8) & 255));
        Peer.Send.Add(static_cast<uint8>(Length & 255));
        Peer.Send.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Length);
        return true;
    }
    auto Base(const TCHAR* Type) -> TSharedRef<FJsonObject>
    {
        auto Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("t"), Type);
        Json->SetNumberField(TEXT("v"), 1);
        Json->SetStringField(TEXT("e"), GuidText(Epoch));
        return Json;
    }
    auto QueueAck(FPeer& Peer, FGuid Wire, ECk_OnlineDirect_Result Result) -> bool
    {
        auto Json = Base(TEXT("ack"));
        Json->SetStringField(TEXT("w"), GuidText(Wire));
        Json->SetNumberField(TEXT("r"), static_cast<int32>(Result));
        return Queue(Peer, Json);
    }
    auto QueueRoster(FPeer& Peer) -> bool
    {
        auto Json = Base(TEXT("roster"));
        TArray<TSharedPtr<FJsonValue>> Entries;
        for (const auto& Member : Snapshot.Get_Members())
        {
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("i"), GuidText(Member.Get_Id()));
            Item->SetStringField(TEXT("n"), Member.Get_Name());
            Item->SetBoolField(TEXT("r"), Member.Get_Ready());
            Entries.Add(MakeShared<FJsonValueObject>(Item));
        }
        Json->SetArrayField(TEXT("m"), Entries);
        return Queue(Peer, Json);
    }
    auto QueueMessage(FPeer& Peer, const FCk_OnlineDirect_Message& Message) -> bool
    {
        auto Json = Base(TEXT("msg"));
        Json->SetNumberField(TEXT("q"), Message.Get_Sequence());
        Json->SetStringField(TEXT("i"), GuidText(Message.Get_SenderId()));
        Json->SetStringField(TEXT("n"), Message.Get_SenderName());
        Json->SetStringField(TEXT("x"), Message.Get_Text());
        return Queue(Peer, Json);
    }
    auto BroadcastRoster() -> void
    {
        for (auto& Peer : Peers) if (Peer->Admitted && !QueueRoster(*Peer)) Peer->Socket->Close();
    }
    auto AddMessage(FGuid Sender, const FString& Name, const FString& Text) -> FCk_OnlineDirect_Message
    {
        FCk_OnlineDirect_Message Message;
        Message.Set_Sequence(++Sequence).Set_SenderId(Sender).Set_SenderName(Name).Set_Text(Text);
        auto& Messages = Snapshot.Get_Messages();
        Messages.Add(Message);
        if (Messages.Num() > 32) Messages.RemoveAt(0);
        Changed();
        return Message;
    }
    auto DropPeer(int32 Index) -> void
    {
        const FGuid Member = Peers[Index]->MemberId;
        const bool Admitted = Peers[Index]->Admitted;
        Peers.RemoveAt(Index);
        if (!Hosting)
        {
            const FString Reason = Snapshot.Get_LastError().IsEmpty() ? TEXT("Connection lost") : Snapshot.Get_LastError();
            FailClient(Reason); return;
        }
        if (Admitted)
        {
            Snapshot.Get_Members().RemoveAll([&](const FCk_OnlineDirect_Member& Entry)
            { return Entry.Get_Id() == Member; });
            Changed();
            BroadcastRoster();
        }
    }
    auto Host(double Now, FGuid Ticket) -> void
    {
        if (!FTransport::ValidateSpec(Spec)) { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        if (Snapshot.Get_State() != ECk_OnlineDirect_State::Offline &&
            Snapshot.Get_State() != ECk_OnlineDirect_State::Failed)
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidState); return; }
        double InviteExpiry = 0;
        if (!TryDeadline(Now, Spec.Get_InviteLifetimeSeconds(), InviteExpiry))
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput, TEXT("Invite deadline cannot advance.")); return; }
        Close(ECk_OnlineDirect_Result::Cancelled);
        Listener = FTcpSocketBuilder(TEXT("CkOnlineDirect"))
            .AsNonBlocking().BoundToPort(static_cast<uint16>(Spec.Get_Port())).Listening(MaxPeers);
        if (!Listener) { Complete(Ticket, ECk_OnlineDirect_Result::TransportError); return; }
        Hosting = true;
        Epoch = FGuid::NewGuid(); Capability = FGuid::NewGuid();
        Expiry = InviteExpiry;
        const FGuid Local = FGuid::NewGuid();
        FCk_OnlineDirect_Member Member;
        Member.Set_Id(Local).Set_Name(Spec.Get_Name()).Set_Ready(false);
        Snapshot.Get_Members().Add(Member);
        Snapshot.Set_State(ECk_OnlineDirect_State::Hosting).Set_LobbyEpoch(Epoch)
            .Set_LocalMemberId(Local).Set_LastError(TEXT(""))
            .Set_JoinDescriptor(FString::Printf(TEXT("ckd:1|%s:%d|%s|%s"),
                *Spec.Get_AdvertisedAddress(), Spec.Get_Port(), *GuidText(Epoch), *GuidText(Capability)));
        Changed(); Complete(Ticket, ECk_OnlineDirect_Result::Succeeded);
    }
    auto Join(const FString& Descriptor, double Now, FGuid Ticket) -> void
    {
        if (!FTransport::ValidateSpec(Spec) || !FTransport::ValidateDescriptor(Descriptor))
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        if (Snapshot.Get_State() != ECk_OnlineDirect_State::Offline &&
            Snapshot.Get_State() != ECk_OnlineDirect_State::Failed)
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidState); return; }
        double Deadline = 0;
        if (!TryDeadline(Now, Spec.Get_OperationTimeoutSeconds(), Deadline))
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput, TEXT("Join deadline cannot advance.")); return; }
        Close(ECk_OnlineDirect_Result::Cancelled);
        FIPv4Address Address; int32 Port = 0; FGuid RequestedEpoch, Key;
        ParseDescriptor(Descriptor, Address, Port, RequestedEpoch, Key);
        FSocket* Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("CkOnlineDirectPeer"), false);
        if (!Socket || !Socket->SetNonBlocking(true))
        {
            if (Socket) ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
            Complete(Ticket, ECk_OnlineDirect_Result::TransportError); return;
        }
        auto Peer = MakeUnique<FPeer>(); Peer->Socket = Socket;
        const FIPv4Endpoint Endpoint(Address, static_cast<uint16>(Port));
        Peer->Connecting = !Socket->Connect(*Endpoint.ToInternetAddr());
        const FGuid Wire = FGuid::NewGuid();
        auto Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("t"), TEXT("join"));
        Json->SetNumberField(TEXT("v"), 1);
        Json->SetStringField(TEXT("e"), GuidText(RequestedEpoch));
        Json->SetStringField(TEXT("k"), GuidText(Key));
        Json->SetStringField(TEXT("w"), GuidText(Wire));
        Json->SetStringField(TEXT("n"), Spec.Get_Name());
        if (!Queue(*Peer, Json)) { Complete(Ticket, ECk_OnlineDirect_Result::TransportError); return; }
        Peers.Add(MoveTemp(Peer));
        Epoch = RequestedEpoch;
        Pending.Add({Wire, Ticket, ECk_OnlineDirect_Action::Join, Deadline});
        JoinWire = Wire;
        Snapshot.Set_State(ECk_OnlineDirect_State::Joining).Set_LastError(TEXT(""));
        Changed();
    }
    auto Execute(ECk_OnlineDirect_Action Action, FGuid Ticket, const FString& Text, bool Ready, double Now) -> void
    {
        if (!FMath::IsFinite(Now) || Now < 0) { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        Now = FMath::Max(Now, LastNow);
        LastNow = Now;
        if (!Ticket.IsValid()) { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        if (Pending.Num() >= MaxPending) { Complete(Ticket, ECk_OnlineDirect_Result::Full); return; }
        if (Action == ECk_OnlineDirect_Action::Host) { Host(Now, Ticket); return; }
        if (Action == ECk_OnlineDirect_Action::Join) { Join(Text, Now, Ticket); return; }
        if (Action == ECk_OnlineDirect_Action::Leave)
        {
            if (Snapshot.Get_State() == ECk_OnlineDirect_State::Offline)
            { Complete(Ticket, ECk_OnlineDirect_Result::InvalidState); return; }
            Close(ECk_OnlineDirect_Result::Cancelled);
            Complete(Ticket, ECk_OnlineDirect_Result::Succeeded); return;
        }
        const bool Active = Snapshot.Get_State() == ECk_OnlineDirect_State::Hosting ||
            Snapshot.Get_State() == ECk_OnlineDirect_State::Joined;
        if (!Active) { Complete(Ticket, ECk_OnlineDirect_Result::InvalidState); return; }
        if (Action == ECk_OnlineDirect_Action::SendText && !FTransport::ValidateText(Text, 240))
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        if (Action != ECk_OnlineDirect_Action::SetReady && Action != ECk_OnlineDirect_Action::SendText)
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput); return; }
        if (Hosting)
        {
            if (Action == ECk_OnlineDirect_Action::SetReady)
            {
                auto& Self = Snapshot.Get_Members()[0];
                if (Self.Get_Ready() != Ready) { Self.Set_Ready(Ready); Changed(); BroadcastRoster(); }
            }
            else
            {
                if (Sequence == MAX_int32)
                { Complete(Ticket, ECk_OnlineDirect_Result::ProtocolError); return; }
                const auto Message = AddMessage(Snapshot.Get_LocalMemberId(), Spec.Get_Name(), Text);
                for (auto& Peer : Peers) if (Peer->Admitted && !QueueMessage(*Peer, Message)) Peer->Socket->Close();
            }
            Complete(Ticket, ECk_OnlineDirect_Result::Succeeded); return;
        }
        if (Peers.IsEmpty()) { Complete(Ticket, ECk_OnlineDirect_Result::TransportError); return; }
        double Deadline = 0;
        if (!TryDeadline(Now, Spec.Get_OperationTimeoutSeconds(), Deadline))
        { Complete(Ticket, ECk_OnlineDirect_Result::InvalidInput, TEXT("Request deadline cannot advance.")); return; }
        const FGuid Wire = FGuid::NewGuid();
        auto Json = Base(Action == ECk_OnlineDirect_Action::SetReady ? TEXT("ready") : TEXT("text"));
        Json->SetStringField(TEXT("w"), GuidText(Wire));
        if (Action == ECk_OnlineDirect_Action::SetReady) Json->SetBoolField(TEXT("r"), Ready);
        else Json->SetStringField(TEXT("x"), Text);
        if (!Queue(*Peers[0], Json)) { Complete(Ticket, ECk_OnlineDirect_Result::TransportError); return; }
        Pending.Add({Wire, Ticket, Action, Deadline});
    }
    auto HandleHost(FPeer& Peer, const TSharedPtr<FJsonObject>& Json, const FString& Type, double Now) -> bool
    {
        FGuid Wire, RequestEpoch;
        if (!GuidField(Json, TEXT("w"), Wire) || !GuidField(Json, TEXT("e"), RequestEpoch)) return false;
        if (Type == TEXT("join"))
        {
            if (Peer.Admitted) return false;
            FString Name; FGuid Key;
            if (!StringField(Json, TEXT("n"), Name) || !IsValidString(Name, 32, 128) ||
                !GuidField(Json, TEXT("k"), Key)) return false;
            ECk_OnlineDirect_Result Result = ECk_OnlineDirect_Result::Succeeded;
            if (RequestEpoch != Epoch || Key != Capability) Result = ECk_OnlineDirect_Result::Unauthorized;
            else if (Now >= Expiry) Result = ECk_OnlineDirect_Result::Expired;
            else if (Snapshot.Get_Members().Num() >= Spec.Get_Capacity()) Result = ECk_OnlineDirect_Result::Full;
            if (Result != ECk_OnlineDirect_Result::Succeeded) return QueueAck(Peer, Wire, Result);
            Peer.MemberId = FGuid::NewGuid(); Peer.Name = Name; Peer.Admitted = true;
            FCk_OnlineDirect_Member Member;
            Member.Set_Id(Peer.MemberId).Set_Name(Name).Set_Ready(false);
            Snapshot.Get_Members().Add(Member); Changed();
            auto Reply = Base(TEXT("joined"));
            Reply->SetStringField(TEXT("w"), GuidText(Wire));
            Reply->SetStringField(TEXT("i"), GuidText(Peer.MemberId));
            if (!Queue(Peer, Reply) || !QueueRoster(Peer)) return false;
            for (const auto& Message : Snapshot.Get_Messages()) if (!QueueMessage(Peer, Message)) return false;
            BroadcastRoster();
            return true;
        }
        if (!Peer.Admitted || RequestEpoch != Epoch) return false;
        if (Peer.WindowStart + 1.0 <= Now) { Peer.WindowStart = Now; Peer.WindowRequests = 0; }
        if (++Peer.WindowRequests > 16) return false;
        if (Peer.Recent.Contains(Wire)) return QueueAck(Peer, Wire, ECk_OnlineDirect_Result::Succeeded);
        if (Type == TEXT("ready"))
        {
            bool Ready = false;
            if (!BoolField(Json, TEXT("r"), Ready)) return false;
            Peer.Recent.Add(Wire); if (Peer.Recent.Num() > 64) Peer.Recent.RemoveAt(0);
            for (auto& Member : Snapshot.Get_Members()) if (Member.Get_Id() == Peer.MemberId)
            { if (Member.Get_Ready() != Ready) { Member.Set_Ready(Ready); Changed(); BroadcastRoster(); } break; }
            return QueueAck(Peer, Wire, ECk_OnlineDirect_Result::Succeeded);
        }
        if (Type == TEXT("text"))
        {
            FString Text;
            if (!StringField(Json, TEXT("x"), Text) || !FTransport::ValidateText(Text, 240)) return false;
            if (Sequence == MAX_int32) return QueueAck(Peer, Wire, ECk_OnlineDirect_Result::ProtocolError);
            Peer.Recent.Add(Wire); if (Peer.Recent.Num() > 64) Peer.Recent.RemoveAt(0);
            const auto Message = AddMessage(Peer.MemberId, Peer.Name, Text);
            for (auto& Other : Peers) if (Other->Admitted && !QueueMessage(*Other, Message)) Other->Socket->Close();
            return QueueAck(Peer, Wire, ECk_OnlineDirect_Result::Succeeded);
        }
        return false;
    }
    auto HandleClient(const TSharedPtr<FJsonObject>& Json, const FString& Type) -> bool
    {
        FGuid PacketEpoch;
        if (!GuidField(Json, TEXT("e"), PacketEpoch) || PacketEpoch != Epoch) return false;
        if (Type == TEXT("ack"))
        {
            FGuid Wire; int32 Code = 0;
            if (!GuidField(Json, TEXT("w"), Wire) || !IntegerField(Json, TEXT("r"), Code) ||
                Code > static_cast<int32>(ECk_OnlineDirect_Result::ProtocolError)) return false;
            const auto Result = static_cast<ECk_OnlineDirect_Result>(Code);
            if (CompleteWire(Wire, Result) && Snapshot.Get_State() == ECk_OnlineDirect_State::Joining)
            {
                Peers[0]->Socket->Close();
                Snapshot.Set_State(ECk_OnlineDirect_State::Failed).Set_LastError(TEXT("Join rejected"));
                Changed();
            }
            return true;
        }
        if (Type == TEXT("joined"))
        {
            FGuid Wire, Member;
            if (!GuidField(Json, TEXT("w"), Wire) || !GuidField(Json, TEXT("i"), Member)) return false;
            if (Snapshot.Get_State() != ECk_OnlineDirect_State::Joining || Wire != JoinWire) return true;
            Snapshot.Set_LocalMemberId(Member).Set_LobbyEpoch(Epoch);
            return true;
        }
        if (Snapshot.Get_State() != ECk_OnlineDirect_State::Joined &&
            !(Snapshot.Get_State() == ECk_OnlineDirect_State::Joining && JoinWire.IsValid() &&
                Snapshot.Get_LocalMemberId().IsValid() && Type == TEXT("roster"))) return false;
        if (Type == TEXT("roster"))
        {
            const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
            if (!Json->TryGetArrayField(TEXT("m"), Items) || !Items || Items->Num() < 1 || Items->Num() > 8) return false;
            TArray<FCk_OnlineDirect_Member> Roster;
            TSet<FGuid> Seen;
            for (const auto& Item : *Items)
            {
                if (!Item || Item->Type != EJson::Object) return false;
                const TSharedPtr<FJsonObject> Entry = Item->AsObject();
                FGuid Id; FString Name; bool Ready = false;
                if (!Entry || !GuidField(Entry, TEXT("i"), Id) ||
                    !StringField(Entry, TEXT("n"), Name) || !IsValidString(Name, 32, 128) ||
                    !BoolField(Entry, TEXT("r"), Ready) || Seen.Contains(Id)) return false;
                Seen.Add(Id);
                FCk_OnlineDirect_Member Member;
                Member.Set_Id(Id).Set_Name(Name).Set_Ready(Ready); Roster.Add(Member);
            }
            if (!Roster.ContainsByPredicate([&](const FCk_OnlineDirect_Member& Item)
                { return Item.Get_Id() == Snapshot.Get_LocalMemberId(); })) return false;
            bool Different = Roster.Num() != Snapshot.Get_Members().Num();
            if (!Different)
            {
                for (int32 I = 0; I < Roster.Num(); ++I)
                {
                    const auto& Old = Snapshot.Get_Members()[I];
                    const auto& New = Roster[I];
                    if (Old.Get_Id() != New.Get_Id() || Old.Get_Name() != New.Get_Name() ||
                        Old.Get_Ready() != New.Get_Ready()) { Different = true; break; }
                }
            }
            if (Different) { Snapshot.Set_Members(Roster); Changed(); }
            if (Snapshot.Get_State() == ECk_OnlineDirect_State::Joining)
            {
                if (!CompleteWire(JoinWire, ECk_OnlineDirect_Result::Succeeded)) return false;
                JoinWire.Invalidate();
                Snapshot.Set_State(ECk_OnlineDirect_State::Joined);
                Changed();
            }
            return true;
        }
        if (Type == TEXT("msg"))
        {
            int32 Number = 0; FGuid Sender; FString Name, Text;
            if (!IntegerField(Json, TEXT("q"), Number) || Number < 1 ||
                !GuidField(Json, TEXT("i"), Sender) || !StringField(Json, TEXT("n"), Name) ||
                !IsValidString(Name, 32, 128) || !StringField(Json, TEXT("x"), Text) ||
                !FTransport::ValidateText(Text, 240)) return false;
            if (Number <= Sequence) return true;
            if (Number != Sequence + 1 && Sequence != 0) return false;
            Sequence = Number;
            FCk_OnlineDirect_Message Message;
            Message.Set_Sequence(Number).Set_SenderId(Sender).Set_SenderName(Name).Set_Text(Text);
            Snapshot.Get_Messages().Add(Message);
            if (Snapshot.Get_Messages().Num() > 32) Snapshot.Get_Messages().RemoveAt(0);
            Changed(); return true;
        }
        return false;
    }
    auto Process(FPeer& Peer, double Now) -> bool
    {
        int32 Frames = 0;
        while (Peer.Receive.Num() >= 4 && Frames++ < 16)
        {
            const uint8* Data = Peer.Receive.GetData();
            const uint32 Length = (uint32(Data[0]) << 24) | (uint32(Data[1]) << 16) |
                (uint32(Data[2]) << 8) | uint32(Data[3]);
            if (Length == 0 || Length > MaxFrame) return false;
            if (Peer.Receive.Num() < static_cast<int32>(Length) + 4) break;
            if (!IsStrictUtf8(Data + 4, Length)) return false;
            const FUTF8ToTCHAR Decoded(reinterpret_cast<const ANSICHAR*>(Data + 4), Length);
            FString Text(Decoded.Length(), Decoded.Get());
            TSharedPtr<FJsonObject> Json;
            const auto Reader = TJsonReaderFactory<>::Create(Text);
            if (!FJsonSerializer::Deserialize(Reader, Json) || !Json) return false;
            FString Type;
            int32 Version = 0;
            if (!StringField(Json, TEXT("t"), Type) ||
                !IntegerField(Json, TEXT("v"), Version) || Version != 1) return false;
            const bool Good = Hosting ? HandleHost(Peer, Json, Type, Now) : HandleClient(Json, Type);
            Peer.Receive.RemoveAt(0, Length + 4, EAllowShrinking::No);
            if (!Good) return false;
        }
        return true;
    }
    auto Poll(double Now) -> void
    {
        if (!FMath::IsFinite(Now)) return;
        Now = FMath::Max(Now, LastNow);
        LastNow = Now;
        for (int32 I = Pending.Num() - 1; I >= 0; --I)
        {
            if (Now < Pending[I].Deadline) continue;
            const bool Joining = Pending[I].Action == ECk_OnlineDirect_Action::Join;
            Complete(Pending[I].Public, ECk_OnlineDirect_Result::TimedOut);
            Pending.RemoveAt(I);
            if (Joining) { FailClient(TEXT("Join timed out")); return; }
        }
        if (Listener)
        {
            for (int32 I = 0; I < 8 && Peers.Num() < MaxPeers; ++I)
            {
                bool Waiting = false;
                if (!Listener->HasPendingConnection(Waiting) || !Waiting) break;
                FSocket* Socket = Listener->Accept(TEXT("CkOnlineDirectIncoming"));
                if (!Socket) break;
                if (!Socket->SetNonBlocking(true)) { Socket->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket); continue; }
                auto Peer = MakeUnique<FPeer>(); Peer->Socket = Socket; Peer->AcceptedAt = Now;
                Peers.Add(MoveTemp(Peer));
            }
        }
        for (int32 I = Peers.Num() - 1; I >= 0; --I)
        {
            FPeer& Peer = *Peers[I];
            bool Failed = false;
            if (Hosting && !Peer.Admitted && Now - Peer.AcceptedAt >= Spec.Get_OperationTimeoutSeconds())
            { DropPeer(I); continue; }
            if (Peer.Connecting)
            {
                const ESocketConnectionState ConnectState = Peer.Socket->GetConnectionState();
                if (ConnectState == SCS_ConnectionError) { DropPeer(I); continue; }
                if (ConnectState != SCS_Connected) continue;
                Peer.Connecting = false;
            }
            if (Peer.SendOffset < Peer.Send.Num())
            {
                int32 Sent = 0;
                if (Peer.Socket->Send(Peer.Send.GetData() + Peer.SendOffset,
                    FMath::Min(Peer.Send.Num() - Peer.SendOffset, 16384), Sent)) Peer.SendOffset += Sent;
                else if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK) Failed = true;
                if (Peer.SendOffset == Peer.Send.Num()) { Peer.Send.Reset(); Peer.SendOffset = 0; }
            }
            if (!Failed)
            {
                uint32 Available = 0;
                bool ReadAny = false;
                for (int32 Read = 0; Read < 4; ++Read)
                {
                    if (!Peer.Socket->HasPendingData(Available) || Available == 0) break;
                    const int32 Count = FMath::Min3(static_cast<int32>(Available), 16384, MaxBuffered - Peer.Receive.Num());
                    if (Count <= 0) { Failed = true; break; }
                    uint8 Buffer[16384]; int32 Received = 0;
                    if (!Peer.Socket->Recv(Buffer, Count, Received) || Received <= 0) { Failed = true; break; }
                    ReadAny = true;
                    Peer.Receive.Append(Buffer, Received);
                }
                if (!Failed && !Process(Peer, Now)) Failed = true;
                if (!Failed && !ReadAny &&
                    Peer.Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::Zero()))
                {
                    uint8 Probe = 0; int32 ProbeCount = 0;
                    const bool Peeked = Peer.Socket->Recv(&Probe, 1, ProbeCount, ESocketReceiveFlags::Peek);
                    // UE's nonblocking Recv also reports a transient would-block as success with zero bytes.
                    // False means EOF/error; would-block is already reported as true by this API.
                    if (!Peeked) { Failed = true; }
                }
            }
            const ESocketConnectionState State = Peer.Socket->GetConnectionState();
            if (State == SCS_ConnectionError || Failed ||
                (!Hosting && Snapshot.Get_State() == ECk_OnlineDirect_State::Failed)) DropPeer(I);
        }
    }
};

auto FTransport::ValidateSpec(const FCk_OnlineDirect_Spec& Spec) -> bool
{
    FIPv4Address Address;
    return IsValidString(Spec.Get_Name(), 32, 128) &&
        ParseIpv4(Spec.Get_AdvertisedAddress(), Address) &&
        Spec.Get_Port() >= 1 && Spec.Get_Port() <= 65535 &&
        Spec.Get_Capacity() >= 2 && Spec.Get_Capacity() <= 8 &&
        FMath::IsFinite(Spec.Get_InviteLifetimeSeconds()) &&
        Spec.Get_InviteLifetimeSeconds() >= 0.1 && Spec.Get_InviteLifetimeSeconds() <= 3600 &&
        FMath::IsFinite(Spec.Get_OperationTimeoutSeconds()) &&
        Spec.Get_OperationTimeoutSeconds() >= 0.1 && Spec.Get_OperationTimeoutSeconds() <= 30;
}
auto FTransport::ValidateText(const FString& Text, int32 MaxCharacters) -> bool
{ return MaxCharacters > 0 && IsValidString(Text, FMath::Min(MaxCharacters, 240), 1024); }
auto FTransport::ValidateDescriptor(const FString& Text) -> bool
{ FIPv4Address Address; int32 Port = 0; FGuid Epoch, Key; return ParseDescriptor(Text, Address, Port, Epoch, Key); }
FTransport::FTransport(const FCk_OnlineDirect_Spec& Spec) : _Impl(MakeUnique<FImpl>(Spec)) {}
FTransport::~FTransport() = default;
auto FTransport::Execute(ECk_OnlineDirect_Action Action, FGuid Ticket,
    const FString& Text, bool Ready, double Now) -> void { _Impl->Execute(Action, Ticket, Text, Ready, Now); }
auto FTransport::Poll(double Now) -> void { _Impl->Poll(Now); }
auto FTransport::Close() -> void { _Impl->Close(ECk_OnlineDirect_Result::Cancelled); }
auto FTransport::DrainCompletions() -> TArray<FTransportCompletion>
{ TArray<FTransportCompletion> Result = MoveTemp(_Impl->Completions); _Impl->Completions.Reset(); return Result; }
auto FTransport::Get_Snapshot() const -> FCk_OnlineDirect_Snapshot { return _Impl->Snapshot; }
}
