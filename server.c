#include "server.h"

//Global Variables [Values Set in main()]
int queue_len           = INVALID;                              //Global integer to indicate the length of the queue
int cache_len           = INVALID;                              //Global integer to indicate the length or # of entries in the cache        [Extra Credit B]
int num_worker          = INVALID;                              //Global integer to indicate the number of worker threads
int num_dispatcher      = INVALID;                              //Global integer to indicate the number of dispatcher threads
uint dynamic_flag       = INVALID_FLAG;                         //Global flag to indicate if the dynamic poool is being used or not         [Extra Credit A]
uint cache_flag         = INVALID_FLAG;                         //Global flag to indicate if the cache is being used or not                 [Extra Credit B]
struct sigaction action;                                        //Global signal handling structure for gracefully terminating from SIGINT
FILE *logfile;                                                  //Global file pointer for writing to log file in worker


/* ************************ Global Hints **********************************/

//int ????      = 0;                                                //[Extra Credit B]  --> If using cache, how will you track which cache entry to evict from array?
int queueSlot_nextReqToremove      = 0;       //[worker()]        --> How will you track which index in the request queue to remove next?
int queueSlot_nextReqReceived      = 0;       //[dispatcher()]    --> How will you know where to insert the next request received into the request queue?
int numOf_reqInQueue               = 0;       //[multiple funct]  --> How will you update and utilize the current number of requests in the request queue?


pthread_t workerThreads[MAX_THREADS];
pthread_t dispatcherThreads[MAX_THREADS];
int workerIDS[MAX_THREADS];
int dispatcherIDS[MAX_THREADS];

pthread_mutex_t lock   = PTHREAD_MUTEX_INITIALIZER;                //What kind of locks will you need to make everything thread safe?  [Hint you need multiple]
pthread_mutex_t logFileLock   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t some_content    = PTHREAD_COND_INITIALIZER;                 //What kind of conditionals will you need to signal different events (i.e. queue full, queue empty)   [Hint you need multiple]
pthread_cond_t free_slot    = PTHREAD_COND_INITIALIZER;


request_t * reqBuffer[MAX_QUEUE_LEN];                                     //How will you track the requests globally between threads? How will you ensure this is thread safe?


/**********************************************************************************/


/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/

/* ************************ Signal Handler Code **********************************/
void gracefulTerminationHandler(int sig_caught) {
  
  /* TODO (D.I)
  *    Description:      Mask SIGINT signal, so the signal handler does not get interrupted (this is a best practice)
  *    Hint:             See Lab Code
  */

  //if another signal occurs while in signal handler, ignore it
  // not sure which of the following 2 lines is the right one
  // sigemptyset(&action.sa_mask); 
  signal(SIGINT, SIG_IGN);

  /* TODO (D.II)
  *    Description:      Print to stdout the number of pending requests in the request queue
  *    Hint:             How should you check the # of remaining requests? This should be a global... Set that number to num_remn_req before print
  */
  int num_remn_req = numOf_reqInQueue;  
  printf("\nGraceful Termination: There are [%d] requests left in the request queue\n", num_remn_req);

  /* TODO (D.III)
  *    Description:      Terminate Server by closing threads, need to close threads before we do other cleanup
  *    Hint:             How should you handle running threads? How will the main function exit once you take care of threads?
  */


  // Wait for all threads to cancel
  for(int i = 0; i < num_worker; i++) {
    pthread_cancel(workerThreads[i]);
  }
  for(int i = 0; i < num_worker; i++) {
    pthread_cancel(dispatcherThreads[i]);
  }
  

  /* TODO (D.IV)
  *    Description:      Close the log file
  */
  if(logfile != NULL) fclose(logfile);

  // reiinstall the signal handler
  sigaction(SIGINT, &action, NULL);
  printf("Done with graceful termination handler.\n");
  
  /* Once you reach here, the thread join calls blocking in main will succeed and the program should terminate */
}
/**********************************************************************************/


/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char *mybuf) {
  /* TODO (Get Content Type)
  *    Description:      Should return the content type based on the file type in the request
  *                      (See Section 5 in Project description for more details)
  *    Hint:             Need to check the end of the string passed in to check for .html, .jpg, .gif, etc.
  */
  char * contentType;

  if(strstr(mybuf, ".htm") != NULL) {
    contentType = (char*)malloc(10);
    strcpy(contentType, "text/html");
  }  else if (strstr(mybuf, ".jpg") != NULL) {
    contentType = (char*)malloc(11);
    strcpy(contentType, "image/jpeg");
  } else if (strstr(mybuf, ".gif") != NULL) {
    contentType = (char*)malloc(10);
    strcpy(contentType, "image/gif");
  } else {
    contentType = (char*)malloc(11);
    strcpy(contentType, "text/plain");
  }
  
  return contentType;
}
 
// Function to open and read the file from the disk into the memory
// Add necessary arguments as needed
char * readFromDisk(int fd, char *mybuf, int * bytesRead) {
  /* TODO (ReadFile.I)
  *    Description:      Try and open requested file, return INVALID if you cannot meaning error
  *    Hint:             Consider printing the file path of your request, it may be interesting and you might have to do something special with it before opening
  *                      If you cannot open the file you should return INVALID, which should be handeled by worker
  */
  char * memory;
  char path[BUFF_SIZE];
  strcpy(path, ".");
  strcat(path, mybuf);
    
  FILE *fp;
  if((fp = fopen(path, "r")) == NULL){
  	printf("Error readFromDisk cannot open the file. \n");
  	return NULL;
  }

  /* TODO (ReadFile.II)
  *    Description:      Find the size of the file you need to read, read all of the contents into a memory location and return the file size
  *    Hint:             Using fstat or fseek could be helpful here
  *                      What do we do with files after we open them?
  */
  
  int file_size;  
  fseek(fp, 0L, SEEK_END);
  file_size = ftell(fp);
  printf("size of file is: %i \n", file_size);
  *bytesRead = file_size;
  
   
  memory = (char *)malloc(file_size);		// allocate memory for fread
  int bytes_read = 0;
  rewind(fp);		// set file position back to the start for fread
  if((bytes_read = fread(memory, sizeof(char),file_size, fp)) == 0){   // read contents into memory
    printf("Error reading from file fp. \n");
  }

  fclose(fp);
  return memory;
}

/**********************************************************************************/

// Function to receive the request from the client and add to the queue
void * dispatch(void *arg) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  EnableThreadCancel();                                         //Allow thread to be asynchronously cancelled
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/ 

  /* TODO (B.I)
  *    Description:      Setup any cleanup handler functions to release any locks and free any memory allocated in this function
  *    Hint:             pthread_cleanup_push(pthread_lock_release,  <address_to_lock>);
  *                      pthread_cleanup_push(pthread_mem_release,   <address_to_mem>);   [If you are putting memory in the cache, who free's it? answer --> cache delete]
  */
  

  int id = -1;

  /* TODO (B.II)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */
  id = *((int*) arg);
  
  printf("%-30s [%3d] Started\n", "Dispatcher", id);

  request_t* request;
  request = (request_t*)malloc(sizeof(request_t));
  char buf[1024];
  request->request = buf;

  pthread_cleanup_push(pthread_lock_release, &lock); // cleanup handler
  pthread_cleanup_push(pthread_mem_release, request); // cleanup handler
  
  while (1) {
    /* TODO (B.III)
    *    Description:      Accept client connection
    *    Utility Function: int accept_connection(void) //utils.h => Line 24
    *    Hint:             What should happen if accept_connection returns less than 0?
    */
    request->fd = accept_connection();

    // !!!!!!!!!!!!!! THIS LINE MAY BE A STICKING POINT !!!!!!!!!!!!!!!!
    if(request->fd < 0) continue;

    /* TODO (B.IV)
    *    Description:      Get request from the client
    *    Utility Function: int get_request(int fd, char *filename); //utils.h => Line 41
    *    Hint:             What should happen if get_request does not return 0?
    */
    if(get_request(request->fd, request->request) == 0) {
      printf("Dispatcher Received Request: fd[%d] request[%s]\n", request->fd, request->request); 
    } else {
      continue;
    }

    /* TODO (B.V)
    *    Description:      Add the request into the queue
    *    Hint:             Utilize the request_t structure in server.h...
    *                      How can you safely add a request to somewhere that other threads can also access? 
    *                      Probably need some synchronization and some global memory... 
    *                      You cannot add onto a full queue... how should you check this? 
    */
    
    // lock to ensure thread safety
    pthread_mutex_lock (&lock);
    // wait until available space in the queue
    while(numOf_reqInQueue == queue_len){
     	pthread_cond_wait (&free_slot, &lock);
    }
    // insert request into next available buffer slot
    reqBuffer[queueSlot_nextReqReceived] = request;

    //increment pointer to next available queue slot, wrapping if at end of buffer
    queueSlot_nextReqReceived = (queueSlot_nextReqReceived == MAX_QUEUE_LEN) ? 0 : queueSlot_nextReqReceived + 1;
    numOf_reqInQueue++;
    pthread_cond_signal(&some_content);
    pthread_mutex_unlock(&lock);
    
  }

  /* TODO (B.VI)
  *    Description:      pop any cleanup handlers that were pushed onto the queue otherwise you will get compile errors
  *    Hint:             pthread_cleanup_pop(0);
  *                      Call pop for each time you call _push... the 0 flag means do not execute the cleanup handler after popping
  */
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  return NULL;
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/ 
}

/**********************************************************************************/

// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  EnableThreadCancel();                                         //Allow thread to be asynchronously cancelled
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/

  
  #pragma GCC diagnostic ignored "-Wunused-variable"      //TODO --> Remove these before submission and fix warnings
  #pragma GCC diagnostic push                             //TODO --> Remove these before submission and fix warnings
  

  // Helpful/Suggested Declaration
  int num_request = 0;        //Integer for tracking each request for printing into the log
  int filesize    = 0;        //Integer for holding the file size returned from readFromDisk or the cache
  char *memory    = NULL;     //memory pointer where contents being requested are read and stored
  int fd          = INVALID;  //Integer to hold the file descriptor of incoming request
  char mybuf[BUFF_SIZE];      //String to hold the file path from the request
  char* content_type;
  char getReqBuf[BUFF_SIZE];
  char * error = "ERROR: Invalid file read";
  
  request_t* incomingReq;
  
  #pragma GCC diagnostic pop                              //TODO --> Remove these before submission and fix warnings

  /* TODO (C.I)
  *    Description:      Setup any cleanup handler functions to release any locks and free any memory allocated in this function
  *    Hint:             pthread_cleanup_push(pthread_lock_release,  <address_to_lock>);
  *                      pthread_cleanup_push(pthread_mem_release,   <address_to_mem>);   [If you are putting memory in the cache, who free's it? answer --> cache delete]
  */

  int id = -1;

  /* TODO (C.II)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */  
  id = *((int*) arg);
   
  pthread_cleanup_push(pthread_lock_release, &lock); // cleanup handler
  pthread_cleanup_push(pthread_lock_release, &logFileLock); // cleanup handler
  pthread_cleanup_push(pthread_mem_release, incomingReq); // cleanup handler
  pthread_cleanup_push(pthread_mem_release, memory); // cleanup handler
  printf("%-30s [%3d] Started\n", "Worker", id);

  while (1) {
    /* TODO (C.III)
    *    Description:      Get the request from the queue
    *    Hint:             You will need thread safe access to the queue... how?
    *                      How will you handle an empty queue? How can you tell dispatch the queue is open? 
    *                      How will you index into the request queue? Global variable probably... How will you update your request queue index?
    *                      IMPORTANT... if you are processing a request you cannot be cancelled... how do you block being cancelled? (see BlockCancelSignal()--> server.h) 
    *                      IMPORTANT... if you are blocking the cancel signal... when do you re-enable it?
    */
    
    pthread_mutex_lock (&lock);
    while(numOf_reqInQueue == 0){
     	pthread_cond_wait (&some_content, &lock);
    }
    BlockCancelSignal();
    incomingReq = reqBuffer[queueSlot_nextReqToremove];
    num_request++;
    queueSlot_nextReqToremove = (queueSlot_nextReqToremove == MAX_QUEUE_LEN) ? 0 : queueSlot_nextReqToremove + 1;
    numOf_reqInQueue--;

    fd = incomingReq->fd;
    strcpy(mybuf, incomingReq->request);
    printf("%s\n", mybuf);
    
    pthread_cond_signal(&free_slot);
    pthread_mutex_unlock(&lock);
    
    /* TODO (C.IV)
    *    Description:      Get the data from the disk
    *    Local Function:   int readFromDisk(//necessary arguments//);
    */
    if((memory = readFromDisk(fd, mybuf, &filesize)) == NULL){
      printf("ERROR: Invalid file read (id: %d).\n", id);
      return_error(fd, "ERROR: Invalid file read");
      pthread_exit(NULL); // deal with invalid situation --> exit thread but not the program
    }

    /* TODO (C.V)
    *    Description:      Log the request into the file and terminal
    *    Utility Function: LogPrettyPrint(FILE* to_write, int threadId, int requestNumber, int file_descriptor, char* request_str, int num_bytes_or_error, bool cache_hit);
    *    Hint:             Call LogPrettyPrint with to_write = NULL which will print to the terminal
    *                      You will need to lock and unlock the logfile to write to it in a thread safe manor
    */
    
    pthread_mutex_lock (&logFileLock);
    char log[50] = {"./webserver_log.txt"};
    if((logfile = fopen(log, "a")) == NULL){
  	  printf("Error worker cannot open the file. \n");
    }
    LogPrettyPrint(logfile, id, num_request, fd, incomingReq->request, filesize, 0);
    LogPrettyPrint(NULL, id, num_request, fd, incomingReq->request, filesize, 0);
    fclose(logfile);
    pthread_mutex_unlock(&logFileLock);
  
    /* TODO (C.VI)
    *    Description:      Get the content type and return the result or error
    *    Utility Function: (1) int return_result(int fd, char *content_type, char *buf, int numbytes); //utils.h => Line 63
    *                      (2) int return_error(int fd, char *buf); //utils.h => Line 75
    *    Hint:             Don't forget to free your memory and set it to NULL so the cancel hanlder does not double free
    *                      You need to focus on what is returned from readFromDisk()... if this is invalid you need to handle that accordingly
    *                      This might be a good place to re-enable the cancel signal... EnableThreadCancel() [hint hint]
    */
    EnableThreadCancel();

    
    content_type = getContentType(mybuf);

    int return_res = 0;
    if((return_res = return_result(fd, content_type, memory, filesize)) == 0){
      printf("returned result successfully\n");
    } else {
      printf("return result failed %s\n", error);
    }
    
    free(content_type);
    content_type = NULL;

  }

  /* TODO (C.VII)
  *    Description:      pop any cleanup handlers that were pushed onto the queue otherwise you will get compile errors
  *    Hint:             pthread_cleanup_pop(0);
  *                      Call pop for each time you call _push... the 0 flag means do not execute the cleanup handler after popping
  */
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  return NULL;
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/
}

/**********************************************************************************/
bool validInput(int port, char * path, int dispatchers, int workers, int dyn_flag, int cash_flag, int q_len, int cash_sz) {
  
  bool goodInput = true;

  // Guard conditions validating input
  // If any input is invalid, inform the user which and 
  // return false
  if(port < MIN_PORT || port > MAX_PORT) {
    goodInput = false;
    printf("ERROR: Invalid port\n");
  }
  if(dispatchers < 1 || dispatchers > MAX_THREADS - workers) {
    goodInput = false;
    printf("ERROR: Invalid # of dispatchers (dispatchers + workers should be <= 100)\n");
  }
  if(workers < 1 || workers > MAX_THREADS - dispatchers) {
    goodInput = false;
    printf("ERROR: Invalid # of workers (dispatchers + workers should be <= 100)\n");
  }
  if(dyn_flag != 1 && dyn_flag != 0) {
    goodInput = false;
    printf("ERROR: Invalid \n");
  }
  if(cash_flag != 1 && cash_flag != 0) {
    goodInput = false;
    printf("ERROR: Invalid cache flag\n");
  }
  if(q_len < 1 || q_len > MAX_QUEUE_LEN) {
    goodInput = false;
    printf("ERROR: Invalid queue length\n");
  }
  if(cash_sz < 1 || cash_sz > MAX_CE) {
    goodInput = false;
    printf("ERROR: Invalid cash size\n");
  }

  FILE *inputFile;
  inputFile = fopen(path, "r");
  if(inputFile == NULL) {
    goodInput = false;
    printf("ERROR: Invalid path\n");
  } else {
    fclose(inputFile);
  }

  return goodInput;
}

void* threadTest() {
  printf("Thread done.\n");
  return NULL;
}

int main(int argc, char **argv) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  // Error check on number of arguments
  if(argc != 9){
    printf("usage: %s port path num_dispatcher num_workers dynamic_flag cache_flag queue_length cache_size\n", argv[0]);
    return -1;
  }

  //Input Variables
  #pragma GCC diagnostic ignored "-Wunused-variable"      //TODO --> Remove these before submission and fix warnings
  #pragma GCC diagnostic push                             //TODO --> Remove these before submission and fix warnings

  int port            = -1;
  char path[PATH_MAX] = "no path set\0";
  num_dispatcher      = -1;                               //global variable
  num_worker          = -1;                               //global variable
  dynamic_flag        = 99999;                            //global variable
  cache_flag          = 99999;                            //global variable
  queue_len           = -1;                               //global variable
  cache_len           = -1;                               //global variable

  #pragma GCC diagnostic pop                              //TODO --> Remove these before submission and fix warnings

  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/

  // Get input args
  port = strtol(argv[1], NULL, 10);
  sprintf(path, "%s", argv[2]);
  num_dispatcher = strtol(argv[3], NULL, 10);
  num_worker = strtol(argv[4], NULL, 10);
  dynamic_flag = strtol(argv[5], NULL, 10);
  cache_flag = strtol(argv[6], NULL, 10);
  queue_len = strtol(argv[7], NULL, 10);
  cache_len = strtol(argv[8], NULL, 10);

  // Perform error checking on input args, terminate on bad input to
  // make sure no stray threads get created
  if(!validInput(port, path, num_dispatcher, num_worker, dynamic_flag, cache_flag, queue_len, cache_len)) {
    printf("ERROR: Invalid input. Terminating.\n");
    return -1;
  }

  /********************* DO NOT REMOVE SECTION - TOP    *********************/
  printf("Arguments Verified:\n\
    Port:           [%d]\n\
    Path:           [%s]\n\
    num_dispatcher: [%d]\n\
    num_workers:    [%d]\n\
    dynamic_flag:   [%s]\n\
    cache_flag:     [%s]\n\
    queue_length:   [%d]\n\
    cache_size:     [%d]\n\n", port, path, num_dispatcher, num_worker, dynamic_flag ? "TRUE" : "FALSE", cache_flag ? "TRUE" : "FALSE", queue_len, cache_len);
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/

  /* TODO (A.III)
  *    Description:      Change SIGINT action for graceful termination
  *    Hint:             Implement gracefulTerminationHandler(), use global "struct sigaction action", see lab 8 for signal handlers
  */
  action.sa_handler = gracefulTerminationHandler;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);


  /* TODO (A.IV)
  *    Description:      Open log file
  *    Hint:             Use Global "File* logfile", use LOG_FILE_NAME as the name, what open flags do you want?
  */
  logfile = fopen(LOG_FILE_NAME, "a");
  if(logfile == NULL) printf("ERROR: Unable to open log file.\n");

  /* TODO (A.V)
  *    Description:      Change the current working directory to server root directory
  *    Hint:             Check for error!
  */
  if(chdir(path) != 0) {
    printf("ERROR: Unable to access input directory. Terminating.\n");
    return -1;
  }


  /* TODO (A.VII)
  *    Description:      Start the server
  *    Utility Function: void init(int port); //utils.h => Line 14
  */
  init(port);
  

  /* TODO (A.VIII)
  *    Description:      Create dispatcher and worker threads (all threads should be detachable)
  *    Hints:            Use pthread_create, you will want to store pthread's globally
  *                      You will want to initialize some kind of global array to pass in thread ID's
  *                      How should you track this p_thread so you can terminate it later? [global]
  */
 for(int i = 0; i < num_worker; i++) {
    dispatcherIDS[i] = i;
    if(pthread_create(&dispatcherThreads[i], NULL, dispatch, &dispatcherIDS[i]) != 0) {
      printf("ERROR: Failed to create dispatcher thread.\n");
    }
  }
  for(int i = 0; i < num_worker; i++) {
    workerIDS[i] = i;
    if(pthread_create(&workerThreads[i], NULL, worker, &workerIDS[i]) != 0) {
      printf("ERROR: Failed to create worker thread.\n");
    }
  }


  /* TODO (A.X)
  *    Description:      Wait for each of the threads to complete their work
  *    Hint:             What can you call that will wait for threads to exit? How can you get threads to exit from ^C (or SIGINT)
  *                      If you are using the dynamic pool flag, you should wait for that thread to exit too
  */
  for(int i = 0; i < num_worker; i++) {
    if(pthread_join(dispatcherThreads[i], NULL) != 0) {
      printf("ERROR: Failed to join dispatcher thread %d.\n", dispatcherIDS[i]);
    }
    printf("Dispatcher %d joined\n", i);
  }
 for(int i = 0; i < num_worker; i++) {
    if(pthread_join(workerThreads[i], NULL) != 0) {
      printf("ERROR: Failed to join worker thread %d.\n", workerIDS[i]);
    }
    printf("Worker %d joined\n", i);
  }

  /* SHOULD NOT HIT THIS CODE UNLESS RECEIVED SIGINT AND THREADS CLOSED */
  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  printf("web_server closing, exiting main\n");
  fflush(stdout);
  return 0;
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/  
}
