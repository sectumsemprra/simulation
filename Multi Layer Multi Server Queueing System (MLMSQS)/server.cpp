#include "server.h"
#include "scheduler.h"
#include <iomanip>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <cmath>

using namespace std;

Server::Server(int id) : SimEntity(id), a_(this), d_(this) {
    queue_ = new Queue();
}

// Exponential function with safety check
double Server::exponential(double mean) {
    if (mean <= 0) {
        cerr << "Error: mean must be positive! Got mean = " << mean << endl;
        return INFINITY;
    }

    double r;
    do {
        r = (double)rand() / (RAND_MAX + 1.0);
    } while (r == 0.0); // Avoid log(0)

    return -log(r) / mean;
}

void Server::initialize() {
    status_ = 0;
    itemArrived_ = 0;
    timeLastEvent = 0.0;

    areaQueue_ = 0.0;
    areaServer_ = 0.0;
    areaSystem_ = 0.0;

    totalQueueDelay_ = 0.0;
    totalSystemDelay_ = 0.0;
    totalCustomerServed = 0;

    // Initialize valid arrival and departure means
    //arrivalMean_ = 0.0;  // Example: Set a valid mean time
    //departureMean_ = 0.0;

    if (getPrev() == nullptr) {
        double t = exponential(arrivalMean_);
        if (t != INFINITY) a_.activate(t);
    }
}

void Server::createTraceFile() {
    string serverID = to_string(getID());
    string fileName = "trace" + serverID + ".out";

    trace_.open(fileName, ios::out);
    if (!trace_) {
        cerr << "Error: Cannot open the trace file.\n";
    }

    trace_ << "Trace file for the simulation\n";
    trace_ << "Format: <event> <time> <item_id> <server_status> <queue_size>\n\n";
}

void Server::arrivalHandler(Item* arrivedItem) {
    itemArrived_++;

    if (itemArrived_ < 100) {
        if (!arrivedItem) {
            arrivedItem = new Item();
            arrivedItem->id_ = itemArrived_;
        }

        arrivedItem->itemArrivalTime = Scheduler::now();
        trace_ << "a\t" << Scheduler::now() << "\t" << arrivedItem->id_ << "\t" << status_ << "\t" << queue_->length() << endl;

        if (status() == 0) {
            // Start processing the item immediately if the server is idle
            status() = 1; // Set server to busy
            itemInService_ = arrivedItem;
            queueDelay_ = Scheduler::now() - itemInService_->itemArrivalTime;
            totalQueueDelay_ += queueDelay_;

            // Schedule the departure after processing
            double t = exponential(departureMean_);
            if (t != INFINITY) {
                d_.activate(t); // Activate departure event
            }
        } else {
            // If server is busy, queue the item
            queue_->enque(arrivedItem);
        }

        // Activate the next arrival event
        double t = exponential(arrivalMean_);
        if (t != INFINITY) {
            a_.activate(t);
        }
    }
}

void Server::departureHandler() {
    if (queue_->length() > 0) {
        // Item has been processed, and there are more items in the queue to be served
        trace_ << "d\t" << Scheduler::now() << "\t" << itemInService_->id_ << "\t" << status_ << "\t" << queue_->length() << endl;
    } else {
        // No more items in the queue
        trace_ << "d\t" << Scheduler::now() << "\t" << itemInService_->id_ << "\t" << 0 << "\t" << queue_->length() << endl;
    }

    // Update total customer served and delays
    totalCustomerServed++;
    systemDelay_ = Scheduler::now() - itemInService_->itemArrivalTime;
    totalSystemDelay_ += systemDelay_;

    // If there are more customers in the queue, continue processing
    if (queue_->length() > 0) {
        itemInService_ = queue_->deque(); // Get next customer from the queue
        queueDelay_ = Scheduler::now() - itemInService_->itemArrivalTime;
        totalQueueDelay_ += queueDelay_;

        trace_ << "s" << getID() << "\t" << Scheduler::now() << "\t" << itemInService_->id_ << "\t" << status_ << "\t" << queue_->length() << endl;

        // Schedule next departure event
        double t = exponential(departureMean_);
        trace_ << t << "que   "<< queue_->length() << endl;
        if (t != INFINITY) {
            d_.activate(t); // Activate departure event
        }

        // If there's a next server, send the item to the next server
        if (this->getNext() != nullptr && this->getID() < 100) {
            sendItem(itemInService_);
        }
    } else {
        // No items left in the queue, set the server to idle
        status_ = 0;
        itemInService_ = nullptr;

        // If there's a next server, send the item to the next server
        if (this->getNext() != nullptr && this->getID() < 100) {
            sendItem(itemInService_);
        }
    }
}

void Server::updateStat() {
    double durationSinceLastEvent = Scheduler::now() - timeLastEvent;
    timeLastEvent = Scheduler::now();

    areaQueue_ += durationSinceLastEvent * queue_->length();
    areaServer_ += durationSinceLastEvent * status();
    areaSystem_ += durationSinceLastEvent * (queue_->length() + status());

    trace_ << areaQueue_ << endl;
}

void Server::report() {
    ofstream report_;
    string serverID = to_string(getID());
    string reportName = "report" + serverID + ".out";
    report_.open(reportName, ios::out);

    if (!report_) {
        cerr << "Error: Cannot open the report file.\n";
        return;
    }

    report_ << fixed << setprecision(8);  // Set precision for floating point numbers
    report_ << "Simulation Report\n";

    // Traffic Intensity: arrivalMean / departureMean
    double trafficIntensity = (arrivalMean_ > 0) ? (arrivalMean_ / departureMean_) : 0.0;
    report_ << "Traffic Intensity: " << trafficIntensity << endl;

    // Average Number of Customers in the Queue
    double avgQueueLength = areaQueue_ / timeLastEvent;  // Assuming areaQueue_ represents total time spent in queue
    report_ << "Average Queue Length: " << avgQueueLength <<" "<<timeLastEvent << endl;

    // Average Server Utilization
    double avgServerUtilization = areaServer_ / timeLastEvent;  // Assuming areaServer_ represents total server usage time
    report_ << "Server Utilization: " << avgServerUtilization << endl;

    // Average Number of Customers in the System
    double avgSystemLength = areaSystem_ / timeLastEvent;  // Assuming areaSystem_ represents total time customers spend in the system
    report_ << "System Length: " << avgSystemLength << endl;

    // Average Queueing Delay
    if (totalCustomerServed > 0) {
        double avgQueueingDelay = totalQueueDelay_ / totalCustomerServed;
        report_ << "Queueing Delay: " << avgQueueingDelay << endl;
    } else {
        report_ << "Queueing Delay: N/A (no customers served)\n";
    }

    // Average System Delay
    if (totalCustomerServed > 0) {
        double avgSystemDelay = totalSystemDelay_ / totalCustomerServed;
        report_ << "System Delay: " << avgSystemDelay << endl;
    } else {
        report_ << "System Delay: N/A (no customers served)\n";
    }

    report_.close();
}
