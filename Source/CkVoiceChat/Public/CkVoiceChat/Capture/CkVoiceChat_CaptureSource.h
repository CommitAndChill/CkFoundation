#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"

#include "CkVoiceChat/VoiceTalker/CkVoiceTalker_Fragment_Data.h"

#include <Containers/Array.h>
#include <Templates/SharedPointer.h>

class IVoiceCapture;

// --------------------------------------------------------------------------------------------------------------------

// The capture seam (spec §7.6): the ONLY surface the talker pipeline sees for microphone input,
// so every automated test runs the full pipeline against the fake and only the final
// "real microphone works" check needs a human. 16-bit mono PCM at the configured sample rate.
class CKVOICECHAT_API ICk_VoiceChat_CaptureSource
{
public:
    virtual ~ICk_VoiceChat_CaptureSource() = default;

    // No device, or a process that renders no audio, is an expected outcome reported through the result - never an
    // ensure. Only a source that returns Started is running.
    virtual auto Start() -> ECk_VoiceChat_CaptureStartResult = 0;
    virtual auto Stop() -> void = 0;

    // Called on the game thread each tick before DrainPcm; scripted sources advance their clock
    // here, device-backed sources have nothing to do.
    virtual auto Tick(FCk_Time InDeltaT) -> void {}

    // Appends all PCM available since the last drain. Called on the game thread each tick.
    virtual auto DrainPcm(TArray<uint8>& OutPcm) -> void = 0;
};

// --------------------------------------------------------------------------------------------------------------------

// Engine microphone capture via FVoiceModule::CreateVoiceCapture. The availability decision is made BEFORE any
// device is touched: a process that renders no audio, or an unattended one, never creates a capture. A disabled
// engine Voice module is a host configuration error ([Voice] bEnabled=true missing from DefaultEngine.ini) and fails
// LOUDLY with one ensure naming the fix; no device, or a device that will not start, is an operational outcome and
// only reported.
class CKVOICECHAT_API FCk_VoiceChat_CaptureSource_Engine : public ICk_VoiceChat_CaptureSource
{
public:
    CK_GENERATED_BODY(FCk_VoiceChat_CaptureSource_Engine);

    explicit FCk_VoiceChat_CaptureSource_Engine(
        int32 InSampleRate);

public:
    auto Start() -> ECk_VoiceChat_CaptureStartResult override;
    auto Stop() -> void override;
    auto DrainPcm(TArray<uint8>& OutPcm) -> void override;

private:
    int32 _SampleRate = 48000;
    TSharedPtr<IVoiceCapture> _Capture;
};

// --------------------------------------------------------------------------------------------------------------------

// Deterministic scripted capture for tests and the gym: a FIFO of segments (sine or silence)
// emitted at real-time pace against the drain calls' accumulated time. Enqueue more segments at
// any point; when the script runs dry the source emits nothing (an open mic with a quiet room is
// silence SEGMENTS, not absence of data — model it explicitly).
class CKVOICECHAT_API FCk_VoiceChat_CaptureSource_Fake : public ICk_VoiceChat_CaptureSource
{
public:
    CK_GENERATED_BODY(FCk_VoiceChat_CaptureSource_Fake);

    explicit FCk_VoiceChat_CaptureSource_Fake(
        int32 InSampleRate);

public:
    auto Start() -> ECk_VoiceChat_CaptureStartResult override;
    auto Stop() -> void override;
    auto Tick(FCk_Time InDeltaT) -> void override;
    auto DrainPcm(TArray<uint8>& OutPcm) -> void override;

public:
    // What every Start() reports (Started by default). Any other value models a machine whose microphone is
    // unavailable, so a test can drive that outcome without depending on - or opening - the real device.
    auto
    Set_StartResult(
        ECk_VoiceChat_CaptureStartResult InStartResult) -> void;

    auto
    Get_StartCount() const -> int32;

    auto
    Get_SamplesEmitted() const -> uint64;

public:
    auto
    Enqueue_Sine(
        FCk_Time InDuration,
        float InAmplitude01,
        float InFrequencyHz) -> void;

    auto
    Enqueue_Silence(
        FCk_Time InDuration) -> void;

    // Advances the fake clock; the next DrainPcm emits this much more audio.
    auto
    AdvanceTime(
        FCk_Time InDeltaT) -> void;

private:
    struct FSegment
    {
        int32 _RemainingSamples = 0;
        float _Amplitude01 = 0.0f;
        float _FrequencyHz = 0.0f;   // 0 = silence
    };

private:
    int32 _SampleRate = 48000;
    bool _IsStarted = false;
    ECk_VoiceChat_CaptureStartResult _StartResult = ECk_VoiceChat_CaptureStartResult::Started;
    int32 _StartCount = 0;
    double _PendingSeconds = 0.0;
    uint64 _SampleCounter = 0;
    TArray<FSegment> _Segments;
};

// --------------------------------------------------------------------------------------------------------------------
