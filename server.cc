#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <vector>
#include "psoogle.h"
#include "simplesocket.h"
#include "url.h"

using namespace std;
 
inverted_index inv_index;
const int numInputs = 3;
  
void process(void * arg) {

  //Create a client socket from the argument and store the GET message
  clientsocket * clSock = (clientsocket *) arg;
  HttpMessage http_msg = clSock->ReceiveHttpMessage();
  string get_msg = http_msg.Resource;

  //Remove the preceding '/' from the request
  get_msg.erase(0, 1);

  //If the request is blank, point it to index.htm
  if(get_msg == "") get_msg = "index.htm";
  
  //If the request is for index.htm or psoogle.jpg, deviler it the file
  //All other requests for files will terminate the connection
  if (get_msg == "index.htm" || get_msg == "psoogle.jpg") {
  	ifstream::pos_type fileSize;
    char * memblock;
	ifstream file (get_msg.c_str(), ios::in | ios::binary | ios::ate);
	if(file.is_open()) {
	  fileSize = file.tellg();
	  memblock = new char [fileSize];
	  file.seekg (0, ios::beg);
	  file.read (memblock, fileSize);
	  file.close();
	  
	  clSock->write( memblock, fileSize);
	}
	else {
	  cout << get_msg << ": File not found!" << endl;
	}
  }
  //Otherwise, see if it matches a properly formatted query
  //If it does, execute the query, if not, close the connection
  else {
  
    //Make the buffer the size of MAX_READ since, well, no address could possibly exceed that size...
    char buf [MAX_READ];
	vector< set<string> > results;

    stringstream stream;
    string token;
	bool correct_query = true;
    stream << get_msg;

	//Query should match following format: index.htm?word1=%s&word2=%s&word3=%s
	getline(stream, token, '?');
	if(token != "index.htm") {
	  correct_query = false;
	}
	for(int i = 1; i < numInputs+1; i++) {
      getline(stream, token, '=');
	  
	  //This is the easiest way I could find to append an int to a string. Obnoxious..
	  ostringstream intConverter;
	  intConverter << i;
	  string compare_token = "word" + intConverter.str();
	  
	  if(token != compare_token) {
	    correct_query = false;
		break;
	  }
	  getline(stream, token, '&');
	  //If the search string is not empty, add its results returned from the inverted index to the total list
	  if(token != "") {
	    results.push_back( inv_index[token] );
	  }
	}
	
	//If it's a valid query, execute it
	//Otherwise, skip to closing the connection
	if(correct_query) {
      set<string>  start = results.back();
      results.pop_back();

	  //Iterate through every element in the first results list (which has already been popped)
      for ( set < string >::iterator it_str = start.begin(); it_str != start.end(); it_str++ ) {
        string url = *it_str;
        bool match = true;
        //Iterate through every remaining result list
		//If the url not present in any of them, it is not a match
        for ( vector< set < string > >::iterator it_set = results.begin(); it_set != results.end(); it_set++ ) {
          set< string > result_list = *it_set;
          if(result_list.count(url) == 0) {
            match = false;
          }
        }
		//If it is a match, send it to the client
        if (match) {
          sprintf (buf, "<a href=\"http://%s\">%s</a></br>", url.c_str(), url.c_str());
          clSock->write(buf, MAX_READ);
        }
      }
    }
  }
  
  clSock->close();
  delete clSock;
}


int main(int argc, char * argv[]) {
  
  //If no parameters are given, it will use my test parameters
  const char* root = "http://www.cs.umass.edu/~bsilva/p4-test/0.html";
  int depth = 3;
  int numThreads = 3;
  int port = 7227;
  
  if(argc == 1) {
    cout << "Using default test parameters..." << endl;
  }
  else if(argc != 5){
    cerr << "Incorrect arguments, exactly 4 parameters required: url, depth, number of threads, port ." << endl;
    exit (-1);
  }
  else {	
    root = argv[1];
    depth = atoi(argv[2]);
    numThreads = atoi(argv[3]);
    port = atoi(argv[4]);
  }
  
  //Begin template code
  inv_index = crawl_and_create_index((char*) root, depth, numThreads);

  cout << endl; 
  for( inverted_index::iterator it_word = inv_index.begin(); it_word != inv_index.end(); it_word++) {
    cout << (*it_word).first << ":\t";
    int cur_filenumber = 0;
    int set_size = (*it_word).second.size();
    for ( set < string >::iterator it_files = (*it_word).second.begin(); it_files != (*it_word).second.end(); it_files++ ) {
      cout << (*it_files);
      if (cur_filenumber != (set_size-1))
        cout << "\n\t";
      else
        cout << "\n";
      cur_filenumber++;
    }
  }
  cout << endl << "indexing complete" << endl;
  //End template code
  
  
  serversocket sock (port);
  
  //No scenarios given where the server would close, so the server must be terminated manually
  while(true) {
    //Wait for a request, spawn a new thread to process the request, start over
	clientsocket * clSock = (clientsocket *) sock.accept();
    pthread_t clientThread_id;
    pthread_create(&clientThread_id, NULL, (void * (*)(void *)) process, (void *) clSock);
  }
}