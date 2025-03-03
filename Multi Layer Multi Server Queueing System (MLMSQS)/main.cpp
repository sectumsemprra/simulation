#include <iostream>
#include <fstream>
using namespace std;

#include "scheduler.h"
#include "server.h"
#include "serviceFacility.h"

int main ()
{
    Scheduler *sch = new Scheduler ();
    sch->initialize ();
    ServiceFacility *servF = new ServiceFacility ();
    servF->createFacility();
    // arrival mean set to 3 instead of 4
    servF->setMean(3.0,3.0, 3.0);
    servF->initializeFacility ();
    sch->run ();
    servF->report();
    return 0;
}
