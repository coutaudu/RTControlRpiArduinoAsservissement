/******************/
/* COUTAUD Ulysse */
/* L3P AII LYON1  */
/* 2022		  */
/******************/

#include "SerialServer.h"

struct sockaddr_in infosSocketClient;

int main(){
    int fileDescriptorSerialPort1;
    // int fileDescriptorSerialPort2;
    int socketUDP;
    command request, response;
    int exit;

    // TODO Open + get EUI + affecte fd1 et fd2.
    fileDescriptorSerialPort1 = openSerial(SERIAL_FILE_1);
   //    fileDescriptorSerialPort2 = openSerial(SERIAL_FILE_2);    

    socketUDP = openUDP();
    exit = getCommandUDP(&request, socketUDP);
    while (!exit) {
	sendCommand(&request, fileDescriptorSerialPort1);
	receiveCommand(&response, fileDescriptorSerialPort1);
	sendResponseToClientUDP(&response);
	if (LOG) logCommand(&request, &response);
	exit = getCommandUDP(&request, socketUDP);
    };

    close(fileDescriptorSerialPort1);
    if(LOG) printf("\tSerial closed by peer.\n");
    return 0;
}

int sendResponseToClientUDP(command* cmd){
    int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(connect(sockfd, (struct sockaddr *)&infosSocketClient, sizeof(infosSocketClient)) < 0) {
	printf("\n Error : Connect Failed \n");
	exit(0);
    }
    send(sockfd, cmd, sizeof(command), 0);
    return 0;
}

int getCommandUDP(command* cmd, int socket){
    int nbBytesReceived;
    socklen_t addrlen;

    if(DEBUG) printf("Get Command From UDP:\t");
    if(DEBUG) fflush(stdout);

    // man: (...) addrlen is a value-result argument (...)
    addrlen = sizeof(infosSocketClient); 
    
    // Recevoir données. ! Appel bloquant ! 
    if ((nbBytesReceived = recvfrom(socket,
				    cmd,
				    sizeof(command),
				    0,
				    (struct sockaddr *) &infosSocketClient,
				    &addrlen))
	== -1) {
	perror("recvfrom()");
    }

    if (DEBUG) {
	struct in_addr ipAddr = ((struct sockaddr_in*)&infosSocketClient)->sin_addr;
	char str[INET_ADDRSTRLEN];
	printf("IPsrc[%s]:%d\n", inet_ntop( AF_INET, &ipAddr, str, INET_ADDRSTRLEN ),((struct sockaddr_in*)&infosSocketClient)->sin_port);
    }

    //    if(LOG) printf("Received Command From UDP.\n");
    if(LOG) printCommand(cmd);
    
    return ( cmd->Version != CURRENT_VERSION );
}

int openUDP(){
    struct sockaddr_in infosSocketServer;
    int socketReceptionUDP;

    if(LOG) printf("\tOpen UDP Port[%5d]\n",PORT);

    if ((socketReceptionUDP=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
	perror("socket");
	return -1;
    }
    if(LOG) printf("\t\tSocket Creation Successful. Socket[%1d]\n",socketReceptionUDP);
    
    // Vide les structures.
    memset((char *) &infosSocketServer, 0, sizeof(infosSocketServer));	
    infosSocketServer.sin_family = AF_INET;
    infosSocketServer.sin_port = htons(PORT);
    infosSocketServer.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if( bind(socketReceptionUDP , (struct sockaddr*)&infosSocketServer, sizeof(infosSocketServer) ) == -1) {
	perror("bind");
	return -1;
    }
    if(LOG) printf("\t\tSocket Bind Successful.\n");
    
    if(LOG) printf("\n");
    return socketReceptionUDP;

}

// TODO Renvoyer en UDP au client.
int logCommand(command* request, command* response){
    if (response->Version!=CURRENT_VERSION){
	printf("\t\t\t[Erreur Version]\n");
	return -1;
    }
    switch (response->Function){
    case INVALID_CMD:
	printf("\t\t\t[Erreur Commande Invalide]\n");
	break;
    case GET_ANALOG:
	printf("\t\t\tA%1u[%3u]\n",request->Argument[0], *((short*)(response->Argument)));
	break;
    case SET_DIGITAL:
	printf("\t\t\tD%1u[%3u]\n",request->Argument[0], response->Argument[0]);
	break;
    case GET_UID:
	printf("\t\t\tUID[%1u]\n",request->Argument[0]);
	break;
    default:
	printf("\t\t\t[Erreur Fonction Inconnue]\n");
    }
    return 0;
}


int receiveCommand(command* cmd, int fdSerial){
    int n;
    if (DEBUG) printf("Start rcv %d bytes:\n",sizeof(command));
    n = rio_readn(fdSerial, cmd, sizeof(command));
    if (DEBUG) printf("Rcvd %d bytes:\t\t",n);
    if (DEBUG) printCommand(cmd);
    return n;
}

int sendCommand(command* cmd, int fdSerial){
    int n;
    n = rio_writen(fdSerial, cmd, sizeof(command));
    if (DEBUG) printf("Send %d bytes:\t\t",n);
    if (DEBUG) printCommand(cmd);
    return n;
}

int openSerial(char* serialFile){
    int fileDescriptor;
    struct termios term;

    if(LOG) printf("\n\tOpen Serial [%s]\n",serialFile);
    while ( (fileDescriptor = open(serialFile, O_RDWR,0))<0) {
	if(LOG) printf("\t\tFailed. Returned FileDescriptor [%d].\n", fileDescriptor);
	if(LOG) printf("\t\t[%s]\n", strerror(errno));
	if(LOG) printf("\t\tWill try again in %d seconds.\n",TEMPO_TRY_AGAIN_OPEN_SERIAL);
	sleep(TEMPO_TRY_AGAIN_OPEN_SERIAL);
    }
    if(LOG) printf("\tOpen Serial successful. FileDescriptor[%d]\n", fileDescriptor);

    // Met en place les parametres série:
    // Serial.begin(115200,SERIAL_8N1);
    if(tcgetattr(fileDescriptor, &term) != 0) {
	printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }


    cfsetspeed(&term, B115200);
    term.c_lflag &= ~ ( ECHO | ECHONL | ISIG | IEXTEN );
    term.c_cflag &= ~ ( PARENB | CSTOPB | CSIZE | CRTSCTS | ICANON );
    term.c_cflag |= CS8 | CREAD | CLOCAL;
    // Disable any special handling of received bytes
    term.c_iflag &= ~(IXON|IXOFF|IXANY|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); 
    term.c_oflag &= ~ ( OPOST | ONLCR ); 
    
    // Une bonne partie pourrai être remplacé par:
    // cfmakeraw(&term); 
    
    if (tcsetattr(fileDescriptor, TCSANOW, &term) != 0) {
	printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    }
    if(LOG) printf("\n");

    
    return fileDescriptor;
  
}


void printCommand(command* cmd){
    printf(" CMD: V[%1d] F[%1d] A0[%3d] A1[%3d]\n",cmd->Version, cmd->Function, cmd->Argument[0], cmd->Argument[1]);
}

// Assumes little endian
void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}
