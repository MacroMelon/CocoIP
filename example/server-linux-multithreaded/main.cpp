#include <iostream>
#include <pthread.h>
#include "cocoip/ipConnection.h"

#define rvr_port 3001
#define ctrl_port 3000

#define ctrlToRvr_msgLen 4
#define rvrToCtrl_msgLen 16

ipConnection rover;
ipConnection controller;

bool quit = false; //main thread stop

volatile int rvr_thread_state = -2;		//0=normal	-1=stopping	-2=stopped
pthread_mutex_t rvr_thread_state_mutex;
volatile int ctrl_thread_state = -2;
pthread_mutex_t ctrl_thread_state_mutex;

pthread_t rvr_hand;
pthread_t ctrl_hand;

volatile char ctrlToRvr_msg[ctrlToRvr_msgLen];
pthread_mutex_t ctrlToRvr_msg_mutex;
volatile char rvrToCtrl_msg[rvrToCtrl_msgLen];
pthread_mutex_t rvrToCtrl_msg_mutex;


volatile bool ctrlToRvr_newMsg = false;				//only send cmd to rover if the recieved message is different
pthread_mutex_t ctrlToRvr_newMsg_mutex;
volatile bool rvrToCtrl_newMsg = false;
pthread_mutex_t rvrToCtrl_newMsg_mutex;

int error;		//pthread error thingy

//*************************		HANDLERS	******************
//TODO- apparently peeking (continuosly poling for data avail) is bad practise.

void *rvr_handler(void*){
	//MUTEX LOCKED
	pthread_mutex_lock(&rvr_thread_state_mutex);
	bool run = (rvr_thread_state == 0);
	pthread_mutex_unlock(&rvr_thread_state_mutex);

	if (!run){
		std::cout<<"RVR thread not starting, thread state not nominal"<<std::endl;
		pthread_exit(NULL);
	}

	char dataIn[4];		//buffer to store incoming data
	char dataOut[4];	//buffer to store outgoing data

	rover.listenAndAcceptClient(rvr_port);		//TODO- handle errors on accepting client etc from cocoip
	std::cout<<"connection detected on rvr port"<<std::endl;

	//HANDSHAKE --- TODO- put into separate function
	int tRet, rRet;
	tRet = rover.sendData("okhi",4);
	if (tRet<0){
		std::cout<<"Send handshake to rover failed. Restarting listenner"<<std::endl;
		rover.cleanUp();
		rvr_handler(NULL);
	}
	rRet = rover.readData(dataIn,4);
	if (rRet<0){
		std::cout<<"Read handshake from rover failed, restarting listenner"<<std::endl;
		rover.cleanUp();
		rvr_handler(NULL);
	}
	if ((dataIn[0]!='g') || (dataIn[1]!='m') || (dataIn[2]!='h') || (dataIn[3]!='i')){
		std::cout<<"AHHHH WRONG HANDSHAKE BYEEE, restarting listenner"<<std::endl;
		rover.sendData("sswh",4);	//server, stop, wrong, handshake (ssnd)
		rover.cleanUp();
		rvr_handler(NULL);
	}


	std::cout<<"Successfully connected to Rover"<<std::endl;

	//MUTEX LOCKED
	pthread_mutex_lock(&rvr_thread_state_mutex);
	run = (rvr_thread_state == 0);
	pthread_mutex_unlock(&rvr_thread_state_mutex);

	while(run){

		if(rover.checkDataAvail()>=4){			//ctrl send data

			rover.readData(dataIn, 4);

			//TODO- properly document frame structure
			/*
			 * rover frame structure
		 	 * dataIn[0] 		--> message to server(s) or controller(c) (also used to identify frame is correct
		 	 * dataIn[1,2,3] 	--> command (q to stop, f for forward, b for back etc)
		 	 */

			/*
			 * server to rover frame structure
			 * dataOUT[0]		--> message from server(s) or from controller(c)
			 * dataOUT[1,2,3]	--> message
			 */

			if(dataIn[0]=='c'){
				//message to ctrl from rover
				//copy from dataIn to shared array *** MUTEX LOCKED ***
				pthread_mutex_lock(&rvrToCtrl_msg_mutex);
				for(int i=0; i<=3; i++ ){
					rvrToCtrl_msg[i] = dataIn[i];
				}
				pthread_mutex_unlock(&rvrToCtrl_msg_mutex);
				pthread_mutex_lock(&rvrToCtrl_newMsg_mutex);
				rvrToCtrl_newMsg=true;
				pthread_mutex_unlock(&rvrToCtrl_newMsg_mutex);
			}
			else if(dataIn[0]=='s'){	//message to server
				/*
				 * message to say bye
				 * message to check if server is responding (handshake)
				 */
				if((dataIn[1]=='a') && (dataIn[2]=='y') && (dataIn[3]=='t')){
					//check if server is there (Server, Are, You, There (sayt)
					rover.sendData("syia",4);		//Server, Yes, I, Am
				}
				else if ((dataIn[1]=='b') && (dataIn[2]=='y') && (dataIn[3]=='e')){
					std::cout<<"Rover says bye, disconnecting and restarting listenner"<<std::endl;
					rover.cleanUp();
					rvr_handler(NULL);
				}

			}
			else{
				std::cout<<"Invalid frame: " <<dataIn<< ". Start byte not c or s. PANICING, restarting listenner."<<std::endl;
				rover.sendData("ssif",4);	//server, stop, invlid, frame (ssif)
				rover.cleanUp();
				rvr_handler(NULL);
			}
		}

		pthread_mutex_lock(&ctrlToRvr_newMsg_mutex);
		bool newData = ctrlToRvr_newMsg;
		pthread_mutex_unlock(&ctrlToRvr_newMsg_mutex);

		if(newData){
			pthread_mutex_lock(&ctrlToRvr_msg_mutex);
			for(int i=0; i<=3; i++ ){
				dataOut[i] = ctrlToRvr_msg[i];
			}
			pthread_mutex_unlock(&ctrlToRvr_msg_mutex);
			//handle new data
			dataOut[0] = 'r'; 	//change to 'c' (from controller) because from controller it is 'r' (to rover)
			rover.sendData(dataOut, 4);		//TODO- Handle error in send call
			std::cout<<"msg to rvr sent"<<std::endl;
			pthread_mutex_lock(&ctrlToRvr_newMsg_mutex);
			ctrlToRvr_newMsg=false;
			pthread_mutex_unlock(&ctrlToRvr_newMsg_mutex);
		}

		pthread_mutex_lock(&rvr_thread_state_mutex);
		run = (rvr_thread_state == 0);
		pthread_mutex_unlock(&rvr_thread_state_mutex);
		std::cout<<">"<<std::endl;
	}

	rover.sendData("ssnd",4);	//server, stop, normal, disconnect (ssnd)
	rover.cleanUp();

	std::cout<<"STOPPED rover handler thread"<<std::endl;

	pthread_mutex_lock(&rvr_thread_state_mutex);
	rvr_thread_state = -2;
	pthread_mutex_unlock(&rvr_thread_state_mutex);

	return NULL;
}

void *ctrl_handler(void*){

	//MUTEX LOCKED
	pthread_mutex_lock(&ctrl_thread_state_mutex);
	bool run = (ctrl_thread_state == 0);
	pthread_mutex_unlock(&ctrl_thread_state_mutex);

	if (!run){
		std::cout<<"CTRL thread not starting, thread state not nominal"<<std::endl;
		pthread_exit(NULL);
	}

	char dataIn[4];		//buffer to store incoming data
	char dataOut[4];	//buffer to store outgoing data

	controller.listenAndAcceptClient(ctrl_port);	//TODO- handle errors on accepting client etc from cocoip
	std::cout<<"connection detected on ctrl port"<<std::endl;

	//HANDSHAKE --- TODO- put into separate function
	int tRet, rRet;
	tRet = controller.sendData("okhi",4);
	if (tRet<0){
		std::cout<<"Send handshake to ctrl failed. Restarting listenner"<<std::endl;
		controller.cleanUp();
		ctrl_handler(NULL);
	}
	rRet = controller.readData(dataIn,4);
	if (rRet<0){
		std::cout<<"Read handshake from ctrl failed, restarting listenner"<<std::endl;
		controller.cleanUp();
		ctrl_handler(NULL);
	}
	if ((dataIn[0]!='g') || (dataIn[1]!='m') || (dataIn[2]!='h') || (dataIn[3]!='i')){
		std::cout<<"AHHHH WRONG HANDSHAKE BYEEE, restarting listenner"<<std::endl;
		controller.sendData("sswh",4);	//server, stop, wrong, handshake (ssnd)
		controller.cleanUp();
		ctrl_handler(NULL);
	}


	std::cout<<"Successfully connected to ctrl"<<std::endl;

	//MUTEX LOCKED
	pthread_mutex_lock(&ctrl_thread_state_mutex);
	run = (ctrl_thread_state == 0);
	pthread_mutex_unlock(&ctrl_thread_state_mutex);

	while(run){

		/*
		 * TODO- WIth this approach, as the same thread sends data from rover to ctrl over the same
		 * 		connection, the telemetry from the rover could get confused with the control packets
		 * 		to the server (ie, if ctrl requests a handshake and this data is in the socket buffer
		 * 		(not proccessed) but the rvr has data to send to ctrl, then the ctrl might recieve not
		 * 		the expected handshake from the server but the frame from the rover which could result
		 * 		in an error. MAJOR BRAINSTORMING REQUIRED
		 */

		if(controller.checkDataAvail()>=4){

			controller.readData(dataIn, 4);

			//TODO- properly document frame structure
			/*
			 * controller frame structure
		 	 * dataIn[0] 		--> message to server(s) or rover(r) (also used to identify frame is correct
		 	 * dataIn[1,2,3] 	--> command (q to stop, f for forward, b for back etc)
		 	 */

			/*
			 * server to ctrl frame structure
			 * dataOUT[0]		--> message from server(s) or from rover(r)
			 * dataOUT[1,2,3]	--> message
			 */

			if(dataIn[0]=='r'){
				//message to rover from ctrl
				//copy from dataIn to shared array *** MUTEX LOCKED ***
				pthread_mutex_lock(&ctrlToRvr_msg_mutex);
				for(int i=0; i<=3; i++ ){
					ctrlToRvr_msg[i] = dataIn[i];
				}
				pthread_mutex_unlock(&ctrlToRvr_msg_mutex);
				pthread_mutex_lock(&ctrlToRvr_newMsg_mutex);
				ctrlToRvr_newMsg=true;
				pthread_mutex_unlock(&ctrlToRvr_newMsg_mutex);
				std::cout<<"msg to rvr"<<std::endl;
			}
			else if(dataIn[0]=='s'){	//message to server
				/*
				 * message to say bye
				 * message to check if server is responding (handshake)
				 */
				if((dataIn[1]=='a') && (dataIn[2]=='y') && (dataIn[3]=='t')){
					//check if server is there (Server, Are, You, There (sayt)
					controller.sendData("syia",4);		//Server, Yes, I, Am
				}
				else if ((dataIn[1]=='b') && (dataIn[2]=='y') && (dataIn[3]=='e')){
					std::cout<<"Controller says bye, disconnecting and restarting listenner"<<std::endl;
					controller.cleanUp();
					ctrl_handler(NULL);
				}

			}
			else{
				std::cout<<"Invalid frame: " <<dataIn<< ". Start byte not r or s. PANICING, restarting listenner."<<std::endl;
				controller.sendData("ssif",4);	//server, stop, invlid, frame (ssif)
				controller.cleanUp();
				ctrl_handler(NULL);
			}
		}

		pthread_mutex_lock(&rvrToCtrl_newMsg_mutex);
		bool newData = rvrToCtrl_newMsg;
		pthread_mutex_unlock(&rvrToCtrl_newMsg_mutex);

		if(newData){
			pthread_mutex_lock(&rvrToCtrl_msg_mutex);
			for(int i=0; i<=3; i++ ){
				dataOut[i] = rvrToCtrl_msg[i];
			}
			pthread_mutex_unlock(&rvrToCtrl_msg_mutex);
			//handle new data
			dataOut[0] = 'r'; 	//change to 'r' (from rover) because from rover it is 'c' (to control)
			controller.sendData(dataOut, 4);		//TODO- Handle error in send call

			pthread_mutex_lock(&rvrToCtrl_newMsg_mutex);
			rvrToCtrl_newMsg=false;
			pthread_mutex_unlock(&rvrToCtrl_newMsg_mutex);
		}

		pthread_mutex_lock(&ctrl_thread_state_mutex);
		run = (ctrl_thread_state == 0);
		pthread_mutex_unlock(&ctrl_thread_state_mutex);
	}

	controller.sendData("ssnd",4);	//server, stop, normal, disconnect (ssnd)
	controller.cleanUp();

	std::cout<<"STOPPED controller handler thread"<<std::endl;

	pthread_mutex_lock(&ctrl_thread_state_mutex);
	ctrl_thread_state = -2;
	pthread_mutex_unlock(&ctrl_thread_state_mutex);

	return NULL;
}

//************	handler start routines	******************

int start_rvr_handler(){
	pthread_mutex_lock(&rvr_thread_state_mutex);
	int state = rvr_thread_state;
	pthread_mutex_unlock(&rvr_thread_state_mutex);

	if(state == -2){
		rvr_thread_state = 0;
		std::cout<<"STARTING rover handler"<<std::endl;
		error = pthread_create(&rvr_hand, NULL, rvr_handler, NULL);
		if (error) {
			std::cout << "Error:unable to create thread," << error << std::endl;
		}
	}
	else{
		std::cout<<"Thread not stopped properly (state not -2). Please execute stop cmd. Thanks!"<<std::endl;
	}
	return 0;
}

int start_ctrl_handler(){
	pthread_mutex_lock(&ctrl_thread_state_mutex);
	int state = ctrl_thread_state;
	pthread_mutex_unlock(&ctrl_thread_state_mutex);

	if(state == -2){
		ctrl_thread_state = 0;
		std::cout<<"STARTING controller handler"<<std::endl;
		error = pthread_create(&ctrl_hand, NULL, ctrl_handler, NULL);
		if (error) {
			std::cout << "Error:unable to create thread," << error << std::endl;
		}
	}
	else{
		std::cout<<"Thread not stopped properly (state not -2). Please execute stop cmd. Thanks!"<<std::endl;
	}

	return 0;
}

//***********************	Handler stop routines	**********************
//TODO- handlers can only be stopped if not listening for connections <- FIX THAT
//TODO- restarting handlers immediately results in undefined behaviour due to ghost sockets...

int stop_rvr_handler(){				//non blocking

	std::cout<<"STOPPING rover handler (dont restart it immediately. Give it a few mins to remove old sockets)"<<std::endl;

	pthread_mutex_lock(&rvr_thread_state_mutex);
	int state = rvr_thread_state;
	pthread_mutex_unlock(&rvr_thread_state_mutex);

	switch(state){
		case 0:
			pthread_mutex_lock(&rvr_thread_state_mutex);
			rvr_thread_state = -1;
			pthread_mutex_unlock(&rvr_thread_state_mutex);
			break;

		case -1:
			std::cout<<"Thread is still halting. Chillax! (Use force stop IF YOU ABSOLUTELY MUST ONLY!)"<<std::endl;
			break;

		case -2:
			std::cout<<"Rover handler already stopped"<<std::endl;
			break;

		default:	//should never reach
			std::cout<<"Wierd.. Invalid thread state (not 0, -1, or -2): "<<state<<std::endl;
			break;
	}

	return 0;
}

int stop_ctrl_handler(){			//non blocking

	std::cout<<"STOPPING controller handler (dont restart it immediately. Give it a few mins to remove old sockets)"<<std::endl;

	pthread_mutex_lock(&ctrl_thread_state_mutex);
	int state = ctrl_thread_state;
	pthread_mutex_unlock(&ctrl_thread_state_mutex);

	switch(state){
		case 0:
			pthread_mutex_lock(&ctrl_thread_state_mutex);
			ctrl_thread_state = -1;
			pthread_mutex_unlock(&ctrl_thread_state_mutex);
			break;

		case -1:
			std::cout<<"Thread is still halting. Chillax! (Use force stop IF YOU ABSOLUTELY MUST ONLY!)"<<std::endl;
			break;

		case -2:
			std::cout<<"Ctrl handler already stopped"<<std::endl;
			break;

		default:	//should never reach
			std::cout<<"Wierd.. Invalid thread state (not 0, -1, or -2): "<<state<<std::endl;
			break;
	}

	return 0;
}

//*********************************** FORCE STOP **************************
//TODO- maybe check if stopped first? or leave that to user?...
int force_stop_rvr_handler(){
	std::cout<<"KILLING rover handler"<<std::endl;
	pthread_cancel(rvr_hand);
	rvr_thread_state = -2;
	rover.cleanUp();
	return 0;
}

int force_stop_ctrl_handler(){
	std::cout<<"KILLING controller handler"<<std::endl;
	pthread_cancel(ctrl_hand);
	ctrl_thread_state = -2;
	controller.cleanUp();
	return 0;
}

/*
 *  COMMANDS:
 * 	quit 		= quit
 * 	start ctrl	= start controller handler
 * 	start rvr	= start rover handler
 * 	stop ctrl 	= stop ctrl
 * 	stop rvr	= stop rover
 * 	force stop 	= force the thread (handler) to terminate
 */

int console_InOut(char *message){

	//********************	quit	*************************

	if ((message[0]=='q') && (message[1]=='u') && (message[2]=='i') && (message[3]=='t')){
		std::cout<<"Bye, bye!"<<std::endl;
		quit = true;
	}

	//*******************	stop X	************************

	else if ((message[0]=='s') && (message[1]=='t') && (message[2]=='o') && (message[3]=='p')){
		if ((message[5]=='r') && (message[6]=='v') && (message[7]=='r')){
			stop_rvr_handler();
		}
		else if ((message[5]=='c') && (message[6]=='t') && (message[7]=='r') && (message[8]=='l')){
			stop_ctrl_handler();
		}
		else{
			std::cout<<"stop WHAT? I didnt get it"<<std::endl;
		}
	}

		//************************	start X	******************************

	else if ((message[0]=='s') && (message[1]=='t') && (message[2]=='a') && (message[3]=='r') && (message[4]=='t')){
		if ((message[6]=='r') && (message[7]=='v') && (message[8]=='r')){
			start_rvr_handler();
		}

		else if ((message[6]=='c') && (message[7]=='t') && (message[8]=='r') && (message[9]=='l')){
			start_ctrl_handler();
		}

		else{
			std::cout<<"start WHAT? I didnt get it"<<std::endl;
		}
	}

	//*************************** FORCE STOP X ******************************************

	else if(((message[0]=='f') && (message[1]=='o') && (message[2]=='r') && (message[3]=='c') && (message[4]=='e'))){
		if((message[5]=='s') && (message[6]=='t') && (message[7]=='o') && (message[8]=='p')){
			if ((message[5]=='r') && (message[6]=='v') && (message[7]=='r')){
				force_stop_rvr_handler();
				std::cout<<"Killed rover handler"<<std::endl;
			}
			else if ((message[5]=='c') && (message[6]=='t') && (message[7]=='r') && (message[8]=='l')){
				force_stop_ctrl_handler();
				std::cout<<"Killed controller handler"<<std::endl;
			}
			else{
				std::cout<<"force stop WHAT? I didnt get it"<<std::endl;
			}
		}
	}

	else{
		std::cout<<"Command not recognised"<<std::endl;
	}
	return 0;
}

//------------------------------------ MAIN THREAD ------------------------------------

int main() {

	//initialise mutual exclusion locks
	pthread_mutex_init(&rvrToCtrl_msg_mutex, NULL);
	pthread_mutex_init(&ctrlToRvr_msg_mutex, NULL);

	pthread_mutex_init(&rvr_thread_state_mutex, NULL);
	pthread_mutex_init(&ctrl_thread_state_mutex, NULL);

	//start handlers
	start_rvr_handler();
	start_ctrl_handler();

	std::cout << "Server for iprover started. Welcome to console!" << std::endl;

	while(!quit){		//interface cli

		/*
		 * 	quit 		= quit
		 * 	start ctrl	= start controller handler
		 * 	start rvr	= start rover handler
		 * 	stop ctrl 	= stop ctrl
		 * 	stop rvr	= stop rover
		 * 	force stop 	= force the thread (handler) to terminate
		 */

		char msg[16];
		std::cin.getline(msg, 16);
		console_InOut(msg);


	}

	stop_rvr_handler();
	stop_ctrl_handler();

	//de-init mutexes
	pthread_mutex_destroy(&ctrlToRvr_msg_mutex);
	pthread_mutex_destroy(&rvrToCtrl_msg_mutex);

	pthread_mutex_destroy(&ctrl_thread_state_mutex);
	pthread_mutex_destroy(&rvr_thread_state_mutex);
	return 0;
}
