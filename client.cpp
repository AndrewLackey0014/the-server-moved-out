// your PA3 client code here
#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>
#include <string>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "TCPRequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

void patient_thread_function (int p_no, BoundedBuffer& request, int n, int m) {
    // functionality of the patient threads

    //take a patient p_no; for n requests , produce a datamsg(p_no,time,ECNO) and push to request buffer
    //          --time dependant on current request
    //          --at 0 -> time == 0.00; at 1 -> time == 0.004; at 2 -> time == 0.008 ...
    double time = 0.00;
    for(int i = 0; i < n; i++){
        //cout << "patient func" << endl;
        char* buf = new char[m];
        datamsg msg(p_no, time, EGCNO);
        memcpy(buf, &msg, sizeof(datamsg)); //load buf with msg
        request.push(buf, sizeof(datamsg)); //push buf to request
        time += 0.004;
        delete[] buf;
        //cout << "made it to end" << endl;
    }
}

void file_thread_function (string filename, int m, BoundedBuffer& request, __int64_t size) {
    // functionality of the file thread

    //file size
    //open output file; allocate memory fseek; close the file
    // while offset < file_size, produce filemsg(offset, m) + filename and push to request buffer
    //      -increment offset; and careful with final message
    int iterations = floor(size/m); 
    int len = sizeof(filemsg) + (filename.size() + 1);
    int offset = 0;
    for(int i = 0; i < iterations; i++){
        filemsg fm(offset, m);
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
		strcpy(buf + sizeof(filemsg), filename.c_str());
        request.push(buf, len);
        offset += m;
        delete[] buf;
    }
    if((size%m) != 0){ //covers the last message 
        int final_offset = size - offset; //size of final transfer
        filemsg fm2(offset, final_offset);
        char* buf = new char[len];
        memcpy(buf, &fm2, sizeof(filemsg));
		strcpy(buf + sizeof(filemsg), filename.c_str());
        request.push(buf, len);
        delete[] buf;
    }


}

void worker_thread_function (BoundedBuffer& request, BoundedBuffer& response, TCPRequestChannel* chan, int m) {
    // functionality of the worker threads

    //forever loop
    //pop message from request buffer
    //view line 120 in server (process request function) for how to decide current message
    //      -send the message accross fifo channel to server
    //      -collect response
    //if data
    //      -create pair of p_no from message and response from server
    //      -push pair to response buffer
    //if file:
    //      -collect filename from the message 
    //      -open file in appropriate mode (read/write/update) open in update
    //      -fseek(SEEK_SET) to offset of the filemsg
    //      -write the buffer from the server
    while(true){
        //cout << "worker func" << endl;
        char* buf = new char[m];
        request.pop(buf, m); //pops msg from request
        MESSAGE_TYPE msg = *((MESSAGE_TYPE*) buf);
        if(msg == DATA_MSG){
            chan->cwrite(buf, sizeof(datamsg));
            double reply;
            chan->cread(&reply, sizeof(double));
            datamsg* data = (datamsg*) buf;
            pair<int, double> pair_resp(data->person ,reply); //load pair for memcpy
            memcpy(buf, &pair_resp, sizeof(pair<int,double>)); //copy pair into buf
            response.push(buf, m); //push buf with pair to response

        }else if(msg == FILE_MSG){

            filemsg fmsg = *((filemsg*) buf);
            FILE * fp;
            string filename = buf + sizeof(filemsg);
            filename = "received/" + filename; // add file location
            fp = fopen(filename.c_str(), "r+"); //open in update
            char* buf2 = new char[m];
            int len = sizeof(filemsg) + (filename.size() + 1);
            chan->cwrite(buf, len);
            chan->cread(buf2, m);
            fseek(fp, fmsg.offset, SEEK_SET);
            fwrite(buf2, 1, fmsg.length, fp);
            fclose(fp);
            delete[] buf2;

        }else if(msg == QUIT_MSG){
            chan->cwrite((char*)&msg, sizeof(MESSAGE_TYPE)); //send quit message to server
            delete[] buf;
            break;
        }
        delete[] buf;

    }

}

void histogram_thread_function (BoundedBuffer& response, HistogramCollection& collection, int n, int m) {
    // functionality of the histogram threads

    //forever loop
    //pop response from response buffer
    //call HC::update(resp->p_no, resp->double)
    while(true){
        //cout << "hist func" << endl;
        char * buf = new char[m];
        response.pop(buf, m);
        pair<int, double> temp;
        memcpy(&temp, buf, sizeof(pair<int,double>)); //copy contents of buf to pair temp
        if (temp.first == -1){ //quit condition
            //cout << "we get exit hist" << endl;
            delete[] buf;
            break;
        }
        collection.update(temp.first, temp.second); //updates collection
        delete[] buf;
    }
}


int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    string ip_address = "127.0.0.1";
    string port_num = "8080";
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:a:r:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
            case 'a':
				ip_address = optarg;
                break;
            case 'r':
				port_num = optarg;
                break;
		}
	}
    
	// fork and exec the server
    // int pid = fork();
    // if (pid == 0) {
    //     execl("./server", "./server", "-m", (char*) to_string(m).c_str(), "-r", (char*) port_num.c_str(), nullptr);
    // }
    
	// initialize overhead (including the control channel)
	TCPRequestChannel* chan = new TCPRequestChannel(ip_address, port_num);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;


    //array / vector of producer threads (if data, p elements, if file, 1 elements)
    //array of histogram threads (if data, h elements, if file, zero elements)
    //array of FIFOS (w elements)
    //array of worker threads (w elements)
    vector<TCPRequestChannel*> vec_tcp;
    

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    //if data:
    //      - create p patient threads (store in producer array)
    //      - create w worker threads (store in worker array)
    //          -create w channel (store in fifo array)
    //      - create h histogram threads (store in hist array)
    // if file
    //      -create file thread(store in producer array)
    //      - create w worker threads (store in worker array)
    //          -create w channel (store in fifo array)

    //if data
    //      -create p patient threads
    //if file
    //      -create 1 file thread
    //create w worker threads
    //      -create w channels
    //if data
    //      -create h hist_threads
    //

    thread * patient_thread = new thread[p];
    thread * worker_thread = new thread[w];
    thread * hist_thread = new thread[h];
    thread * file_thread = new thread[1];

    // MESSAGE_TYPE new_chan_msg = NEWCHANNEL_MSG;
    // for(int i = 0; i < w; i++){ //create w channels
    //     //cout << "we here?" << endl;
    //     char* chan_buf = new char[m];
    //     chan->cwrite(&new_chan_msg, sizeof(MESSAGE_TYPE)); // request new channel from server
    //     string chan_name; 
    //     chan->cread(chan_buf, sizeof(string)); // read channel name from server
    //     chan_name = chan_buf;
    //     FIFORequestChannel* new_chan = new FIFORequestChannel(chan_name, FIFORequestChannel::CLIENT_SIDE);
    //     vec_fifo.push_back(new_chan); //load channel in fifo vec
    //     delete[] chan_buf;
    // }
    
    for(int i = 0; i < w; i++){ //create w worker channels
        TCPRequestChannel* new_chan = new TCPRequestChannel(ip_address, port_num);
        vec_tcp.push_back(new_chan);
    }

    if(f == ""){ // data operation
        for(int i = 0; i < p; i++){
            patient_thread[i] = thread(patient_thread_function, (i+1), ref(request_buffer), n, m);
        }

        for(int i = 0; i < h; i++){
            hist_thread[i] = thread(histogram_thread_function, ref(response_buffer), ref(hc), n, m);
        }
    }

    if(f != ""){ //file operation
        FILE * fp;
        string filename = "received/" + f;
        fp = fopen(filename.c_str(), "w+");
        filemsg fm(0,0);
        string fname = f;
        int len = sizeof(filemsg) + (fname.size() + 1);
        char* buf = new char[len];
        memcpy(buf, &fm, sizeof(filemsg));
		strcpy(buf + sizeof(filemsg), fname.c_str());
        chan->cwrite(buf, len);  // I want the file length;
		__int64_t size; //size of file
		chan->cread(&size, sizeof(__int64_t));
        file_thread[0] = thread(file_thread_function, f, m, ref(request_buffer), size);
        delete[] buf;
        fclose(fp);
    }

    for(int i = 0; i < w; i++){
        TCPRequestChannel* temp = vec_tcp[i];
        worker_thread[i] = thread(worker_thread_function, ref(request_buffer), ref(response_buffer), temp, m);
    }

	/* join all threads here */
    //iterate over all thread arrays calling join
    //      -order is important; producer before consumer
    if(f != ""){ 
        file_thread[0].join();
    }
    if(f == ""){
        for(int i = 0; i < p; i++){
            patient_thread[i].join();
        }
    }
    MESSAGE_TYPE quit = QUIT_MSG;
    for(int i = 0; i < w; i++){// load quit for worker threads
        request_buffer.push((char*) &quit, sizeof(MESSAGE_TYPE));
    }

    for(int i = 0; i < w; i++){
        worker_thread[i].join();
    }

    if(f == ""){ //loads quit condition in hist
        for(int i = 0; i < h; i++){
            pair<int,double> end;
            end.first = -1;
            end.second = -1;
            response_buffer.push((char*) &end, sizeof(pair<int,double>));
        }

        for(int i = 0; i < h; i++){
            hist_thread[i].join();
        }
    }

    delete[] patient_thread;
    delete[] worker_thread;
    delete[] hist_thread;
    delete[] file_thread;
    //cleanup

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

    //quit and close all channels in fifo array
	// quit and close control channel
    MESSAGE_TYPE q = QUIT_MSG;
    for(int i = 0; i < w; i++){//quits all fifo
        vec_tcp[i]->cwrite((char *) &q, sizeof(MESSAGE_TYPE));
        delete vec_tcp[i];
    }
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}
