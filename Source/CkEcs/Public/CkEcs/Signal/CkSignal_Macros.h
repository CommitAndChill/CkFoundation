#pragma once

#include "CkEcs/Signal/CkSignal_Fragment.h"
#include "CkEcs/Signal/CkSignal_Utils.h"

// --------------------------------------------------------------------------------------------------------------------
// Granular macros — prefer the 'meta' macros further below.
//
// _API_ is accepted for source compatibility and deliberately NOT applied. Every type these macros declare derives from a
// header-only template whose members (the AngelScript property registrars included) are instantiated in whichever module
// uses it. A dllexport on the derived struct makes MSVC instantiate and export the base's members from THAT module, so
// two modules declaring a signal with the same payload (CkEcs' OnEntityBeginDestroy and any other FCk_Handle signal) both
// export `TFragment_Signal<FCk_Handle>::RegisterAngelScriptProperty_*` and the second fails to link with LNK2005 against
// the first's import library (CkNavigation on UE 5.7.4, PCH off). Before the UE 5.8 declaration sweep the export sat before
// the class-key, where the compiler ignored it, so nothing was ever exported and nothing needs to be.

#define CK_DEFINE_SIGNAL(_API_, _SignalName_, ...)\
    struct FFragment_Signal_##_SignalName_ \
    : public ck::TFragment_Signal<__VA_ARGS__> {}

#define CK_DEFINE_SIGNAL_UTILS(_API_, _SignalName_)\
    class UUtilsSignal_##_SignalName_ \
    : public ck::TUtils_Signal<FFragment_Signal_##_SignalName_> {}

#define CK_DEFINE_SIGNAL_WITH_DELEGATE(_API_, _SignalName_, _DelegateName_, ...)\
    struct FFragment_Signal_Delegate_##_SignalName_ \
    : public ck::TFragment_Signal_Delegate<_DelegateName_, ECk_Signal_PostFireBehavior::DoNothing, __VA_ARGS__> {};\
    struct FFragment_Signal_Delegate_##_SignalName_##_PostFireUnbind \
    : public ck::TFragment_Signal_Delegate<_DelegateName_, ECk_Signal_PostFireBehavior::Unbind, __VA_ARGS__> {}

#define CK_DEFINE_SIGNAL_WITH_DELEGATE_UTILS(_API_, _SignalName_)\
    class UUtils_Signal_##_SignalName_ \
    : public ck::TUtils_Signal_Delegate<FFragment_Signal_##_SignalName_, FFragment_Signal_Delegate_##_SignalName_> {};\
    class UUtils_Signal_##_SignalName_##_PostFireUnbind\
    : public ck::TUtils_Signal_Delegate<FFragment_Signal_##_SignalName_, FFragment_Signal_Delegate_##_SignalName_##_PostFireUnbind> {}

// --------------------------------------------------------------------------------------------------------------------

// Define Signal that works ONLY within C++
#define CK_DEFINE_SIGNAL_AND_UTILS(_API_, _SignalName_, ...)\
    CK_DEFINE_SIGNAL(_API_, _SignalName_, __VA_ARGS__)\
    CK_DEFINE_SIGNAL_UTILS(_API_, _SignalName_)\

// Define Signal that works in C++ AND supports Unreal Delegates with the same interface
#define CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(_API_, _SignalName_, _Delegate_, ...)\
    CK_DEFINE_SIGNAL(_API_, _SignalName_, __VA_ARGS__);\
    CK_DEFINE_SIGNAL_WITH_DELEGATE(_API_, _SignalName_, _Delegate_, __VA_ARGS__);\
    CK_DEFINE_SIGNAL_WITH_DELEGATE_UTILS(_API_, _SignalName_)

#define CK_SIGNAL_BIND_PROMISE(_SignalUtils_, _Handle_, _PromiseDelegate_)\
    CK_SIGNAL_BIND(_SignalUtils_, _Handle_, _PromiseDelegate_, ECk_Signal_BindingPolicy::FireIfPayloadInFlight, ECk_Signal_PostFireBehavior::Unbind)\

#define CK_SIGNAL_BIND_REQUEST_FULFILLED(_SignalUtils_, _Handle_, _PromiseDelegate_)\
    CK_SIGNAL_BIND(_SignalUtils_, _Handle_, _PromiseDelegate_, ECk_Signal_BindingPolicy::IgnorePayloadInFlight, ECk_Signal_PostFireBehavior::Unbind)\

#define CK_SIGNAL_BIND(_SignalUtils_, _Handle_, _Delegate_, _BindingPolicy_, _PostFireBehavior_)\
if (_PostFireBehavior_ == ECk_Signal_PostFireBehavior::DoNothing) \
{ _SignalUtils_::Bind(_Handle_, _Delegate_, _BindingPolicy_); } \
else\
{ _SignalUtils_##_PostFireUnbind::Bind(_Handle_, _Delegate_, _BindingPolicy_); }

#define CK_SIGNAL_BIND_WITH_CONDITION(_SignalUtils_, _Handle_, _Delegate_, _BindingPolicy_, _PostFireBehavior_, _PredicateFunc_)\
if (_PostFireBehavior_ == ECk_Signal_PostFireBehavior::DoNothing) \
{ _SignalUtils_::Bind(_Handle_, _Delegate_, _BindingPolicy_, _PredicateFunc_); } \
else\
{ _SignalUtils_##_PostFireUnbind::Bind(_Handle_, _Delegate_, _BindingPolicy_, _PredicateFunc_); }

#define CK_SIGNAL_UNBIND(_SignalUtils_, _Handle_, _Delegate_)\
_SignalUtils_::Unbind(_Handle_, _Delegate_)

// --------------------------------------------------------------------------------------------------------------------
