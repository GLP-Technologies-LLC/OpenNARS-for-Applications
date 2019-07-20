#include "Decision.h"

//Inject action event after execution or babbling
void InjectActionEvent(Decision *decision)
{
    assert(decision->operationID > 0, "Operation 0 is reserved for no action");
    decision->op = operations[decision->operationID-1]; //TODO put into InjectActionEvent
    (*decision->op.action)();
    ANSNA_AddInputBelief(decision->op.sdr, "SomeOpID");
}

//"reflexes" to try different operations, especially important in the beginning
Decision MotorBabbling()
{
    Decision decision = (Decision) {0};
    int n_ops = 0;
    for(int i=0; i<OPERATIONS_MAX && operations[i].action != 0; i++)
    {
        n_ops = i+1;
    }
    if(n_ops > 0)
    {
        decision.operationID = 1+(rand() % (n_ops));
        IN_DEBUG (
            printf(" ANSNA BABBLE %d\n", decision.operationID);
        )
        decision.execute = true;
    }
    return decision;
}

Decision RealizeGoal(Event *goal, long currentTime)
{
    Decision decision = (Decision) {0};
    int closest_postcon_concept_i;
    if(Memory_getClosestConcept(&goal->sdr, goal->sdr_hash, &closest_postcon_concept_i))
    {
        Concept *postcon_c = concepts.items[closest_postcon_concept_i].address;
        double bestTruthExpectation = 0;
        int newestOccurrenceTime = 0; //TODO remove
        Implication debugImp = {0};
        Concept *precon_concept;
        for(int i=1; i<OPERATIONS_MAX; i++)
        {
            if(operations[i-1].action == 0)
            {
                break;
            }
            for(int j=0; j<postcon_c->precondition_beliefs[i].itemsAmount; j++)
            {
                Implication imp = postcon_c->precondition_beliefs[i].array[j];
                IN_DEBUG (
                    printf("CONSIDERED IMPLICATION: %s\n", imp.debug);
                    SDR_PrintWhereTrue(&imp.sdr);
                )
                //now look at how much the precondition is fulfilled
                int closest_precon_concept_i;
                if(Memory_getClosestConcept(&imp.sdr, imp.sdr_hash, &closest_precon_concept_i))
                {
                    Concept * current_precon_c = concepts.items[closest_precon_concept_i].address;
                    Event * precondition = FIFO_GetNewestElement(&current_precon_c->event_beliefs); //a. :|:
                    Event ContextualOperation = Inference_GoalDeduction(goal, &imp); //(&/,a,op())!
                    double operationGoalTruthExpectation = precondition->truth.confidence * ContextualOperation.truth.confidence; //Truth_Expectation(Truth_Deduction(ContextualOperation.truth, precondition->truth)); //op()! //TODO project to now
                    //if(operationGoalTruthExpectation > bestTruthExpectation)
                    if(precondition->occurrenceTime > newestOccurrenceTime)
                    {
                        IN_DEBUG (
                            printf("CONSIDERED PRECON: %s\n", current_precon_c->debug);
                            printf("CONSIDERED PRECON truth ");
                            Truth_Print(&precondition->truth);
                            printf("CONSIDERED goal truth ");
                            Truth_Print(&goal->truth);
                            printf("CONSIDERED imp truth ");
                            Truth_Print(&imp.truth);
                            printf("CONSIDERED time %d\n", (int)precondition->occurrenceTime);
                            SDR_PrintWhereTrue(&current_precon_c->sdr);
                            SDR_PrintWhereTrue(&precondition->sdr);
                        )
                        newestOccurrenceTime = precondition->occurrenceTime;
                        precon_concept = current_precon_c;
                        debugImp = imp;
                        decision.operationID = i;
                        bestTruthExpectation = operationGoalTruthExpectation;
                    }
                }
            }
        }
        if(decision.operationID == 0 || bestTruthExpectation < DECISION_THRESHOLD)
        {
            IN_OUTPUT( printf("below decision thres %f\n", bestTruthExpectation); )
            return decision;
        }
        IN_DEBUG (
            printf("%s %f,%f",debugImp.debug, debugImp.truth.frequency, debugImp.truth.confidence); //++
            printf("\n"); //++
            printf("SELECTED PRECON: %s\n", precon_concept->debug); //++
            printf(debugImp.debug); //++
            printf(" ANSNA TAKING ACTIVE CONTROL %d\n", decision.operationID);
        )
        decision.execute = true;
    }
    return decision;
}

void Decision_Making(Event *goal, long currentTime)
{
    Decision decision = {0};
    //try motor babbling with a certain chance
    if(!decision.execute && rand() % 1000000 < (int)(MOTOR_BABBLING_CHANCE*1000000.0))
    {
        decision = MotorBabbling();
    }
    //try matching op if didn't motor babble
    if(!decision.execute)
    {
        decision = RealizeGoal(goal, currentTime);
    }
    if(decision.execute)
    {
        InjectActionEvent(&decision);
    }
}
