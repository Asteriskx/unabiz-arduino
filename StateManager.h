#ifndef UNABIZ_ARDUINO_STATEMANAGER_H
#define UNABIZ_ARDUINO_STATEMANAGER_H

// To turn on debug messages:
// #define DEBUG_STATE

static const uint8_t stepStart = 1;
static const uint8_t stepListen = 2;
static const uint8_t stepSend = 3;
static const uint8_t stepReceive = 4;
static const uint8_t stepPower = 5;
static const uint8_t stepTimeout = 6;
static const uint8_t stepEnd = 7;
static const uint8_t stepTest1 = 91;
static const uint8_t stepTest2 = 92;
static const uint8_t stepTest3 = 93;
static const uint8_t stepTest4 = 94;

static const uint8_t stepSuccess = 101;
static const uint8_t stepFailure = 102;

struct FunctionState {
  //  Represents the state of a function.  Remembers the current step, next step, delay
  //  and the success/failure status.
  String functionName = "";
  FunctionState *parentState = 0;
  FunctionState *childState = 0;
  uint8_t currentStep = 0;
  uint8_t nextStep = 0;
  unsigned long delayUntil = 0;

  //  Store the function variables here so they can be restored.
  int int1 = 0;
  uint8_t uint1 = 0;
  unsigned long ulong1 = 0;
  String string1 = "";
};

class StateManager {
  //  Manages the states of a function and the functions that it calls.  Called by the
  //  Finite State Machine to run each function as multiple steps.  The StateManager
  //  enables functions to suspend and resume while incomplete.
public:
  ~StateManager() {
    destroy();
  }

  void init() {
    //  Deallocate any function states previously created.
    destroy();
  }

  void destroy() {
    //  Deallocate any function states previously created.
    if (!currentState) return;
    FunctionState *childState = currentState->childState;
    if (childState) {
      delete childState;
      currentState->childState = 0;
    }
    delete currentState;
    currentState = 0;
  }

  uint8_t getStatus() {
    //  Return stepSuccess if completed successfully.  Return stepFailure if failed.
    if (!currentState) return 0;
    return currentState->currentStep;
  }

  uint32_t resetDelay() {
    //  Return the delay requested and reset to 0.
    unsigned long delayUntil = currentState->delayUntil;
    currentState->delayUntil = 0;
    return delayUntil;
  }

  uint8_t begin(const String &functionName, uint8_t firstStep) {
    //  Called at the start of a function to begin the function state.
    //  If called the first time, we create a new function state.
    //  Else we reuse the function state previously created.
    //  Return the current step for this function.
    if (!currentState) {
      //  Create the root state if not found.
      currentState = createState(functionName, firstStep, 0);
    } else if (currentState->functionName == functionName) {
      //  Reuse the root state. Do nothing.
#ifdef DEBUG_STATE
      Serial.print(F("## Resume state ")); Serial.println(functionName + ", step " + currentState->currentStep);
#endif // DEBUG_STATE
    } else {
      FunctionState *childState = currentState->childState;
      if (!childState || childState->functionName != functionName) {
        //  If we don't have a child state for this function name, create a child state.
        if (childState) delete childState;
        childState = createState(functionName, firstStep, currentState);
        currentState->childState = childState;
        currentState = childState;
      } else {
        //  Else switch to the child state.
        currentState = childState;
#ifdef DEBUG_STATE
        Serial.print(F("## Resume state ")); Serial.println(functionName + ", step " + currentState->currentStep);
#endif // DEBUG_STATE
      }
    }
    //  Return the current step for this function.
    return currentState->currentStep;
  }

  //  Set and retrieve the suspended values for the function.
  void setState(int int1) { currentState->int1 = int1; }
  void setState(uint8_t uint1) { currentState->uint1 = uint1; }
  void setState(unsigned long ulong1) { currentState->ulong1 = ulong1; }
  void setState(const String &string1) { currentState->string1 = string1; }

  void getState(int &int1) { int1 = currentState->int1; }
  void getState(uint8_t &uint1) { uint1 = currentState->uint1; }
  void getState(unsigned long &ulong1) { ulong1 = currentState->ulong1; }
  void getState(String &string1) { string1 = currentState->string1; }

  bool suspend(uint8_t nextStep, uint32_t delay = 0) {
    //  Suspend the current function and continue execution later at that step
    //  after that delay.
    //  Remember the next step and pop the current state.
#ifdef DEBUG_STATE
    Serial.print(F("## Suspend state ")); Serial.println(currentState->functionName + ", step " + currentState->currentStep + ", next step " + nextStep);
#endif // DEBUG_STATE
    popState(currentState->currentStep, nextStep);
    if (delay > 0) currentState->delayUntil = millis() + delay;
    //  Check for transitions if the child has completed.
    transitionState();
    if (currentState->currentStep == stepFailure) return false;
    return true;
  }

  bool suspend() {
    //  Suspend the current function and continue execution later at the same state.
    //  Pop the current state and stay in the same state.
    return suspend(currentState->currentStep);
  }

  bool end(bool status = true) {
    //  Called at the end of a function to terminate the function state
    //  and return a success or failure status, depending on the status parameter (default to true)
    //  Status will be propagated to the parent function state.  Returns the status value.
    if (status) popState(stepSuccess);
    else popState(stepFailure);
    //  Check for transitions if the child has completed.
    transitionState();
    return status;
  }

private:
  FunctionState *createState(const String &functionName, uint8_t firstStep, FunctionState *parentState) {
    //  Create a new function state.
#ifdef DEBUG_STATE
    Serial.print(F("## New state ")); Serial.println(functionName);
#endif // DEBUG_STATE
    FunctionState *state = new FunctionState();
    state->functionName = functionName;
    state->currentStep = firstStep;
    state->parentState = parentState;
    return state;
  }

  void popState(uint8_t currentStep0, uint8_t nextStep0 = 0) {
    //  Pop the current function state and resume with the parent function state.
    if (!currentState) return;
    //  Save the currrent and next steps and transition later when the child has completed.
    currentState->currentStep = currentStep0;
    currentState->nextStep = nextStep0;
    //  If current state has no child, proceed to next step.
    if (nextStep0 && !currentState->childState) {
      // Serial.print(F("Go to next step: ")); Serial.println(nextStep0);
      currentState->currentStep = nextStep0;
      currentState->nextStep = 0;
    }
    //  Make parent state the current state, if the parent exists.
    if (currentState->parentState) {
      currentState = currentState->parentState;
    }
#ifdef DEBUG_STATE
    Serial.print(F("## Pop to parent state: ")); Serial.println(currentState->functionName);
#endif // DEBUG_STATE
  }

  bool transitionState() {
    //  Transition the current state to next step if child has completed.
    //  Propate the child delay to current state.

    //  If child state has completed, delete child state and move to next step.
    //  Return true if we have transitioned.
    FunctionState *childState = currentState->childState;
    if (!childState) return false;
    //  If child requested for delay, propagate to root and wait at the Finite State Machine.
    if (childState->delayUntil) {
      currentState->delayUntil = childState->delayUntil;
      childState->delayUntil = 0;
    }
    //  Child has not finished yet, return.
    if (childState->currentStep != stepSuccess && childState->currentStep != stepFailure) return false;

    if (childState->currentStep == stepFailure) {
      //  If child has failed, mark my state as failed too.  This will propagate to root.
      Serial.print(F("###### STEP FAILED: ")); Serial.println(childState->functionName);
      currentState->currentStep = stepFailure;
    }
    else {
      //  If child has succeeded, proceed to the next step.
      // Serial.print(F("Child has succeeded: ")); Serial.println(currentState->functionName);
      currentState->currentStep = currentState->nextStep;
      currentState->nextStep = 0;
    }
    //  Delete the child.
#ifdef DEBUG_STATE
    Serial.print(F("## Delete state: ")); Serial.println(childState->functionName);
#endif // DEBUG_STATE
    delete childState;
    currentState->childState = 0;
    return true;
  }

  FunctionState *currentState = 0;
};

#endif // UNABIZ_ARDUINO_STATEMANAGER_H