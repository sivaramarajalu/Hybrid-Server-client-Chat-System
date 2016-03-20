/**
 * @sramadur_assignment1
 * @author  sivaramarajalu ramadurai venkataraajalu <sramadur@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include<arpa/inet.h>  /* inet_ntoa() to format IP address */
#include<netinet/in.h> /* in_addr structure */
#include<ifaddrs.h>

#include "../include/global.h"
#include "../include/logger.h"

#define MAXDATASIZE 275
#define STDIN 0

typedef enum {
	false, true
} bool;

char localIp[INET6_ADDRSTRLEN];
char *ubit_name = "sramadur";

struct ListNode {
	int sockfd;
	int port;
	int sent;
	int recieved;
	char myip[INET6_ADDRSTRLEN];
	char myhostname[50];
	char *message[102];
	int storedmsgcount;
	bool isOnline; //for online or offline
	struct ListNode *next;
	char *blockednodes[5]; // = {"a","a","a","a","a"};//for storing blocked ip addresses
};

struct LinkedList {
	struct ListNode *head;
	int size;
};

//function returns ip address pertaining to its type V4 or V6
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*) sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

/*
 //Function to handle setsockopt(), for reusing address
 void sigchld_handler(int s)
 {
 // waitpid() might overwrite errno, so we save and restore it:
 int saved_errno = errno;
 while(waitpid(-1, NULL, WNOHANG) > 0);
 errno = saved_errno;
 }
 */

/* Function sets the global variable "localIp" to public IP address of the current machine
 reference - Code taken from http://man7.org/linux/man-pages/man3/getifaddrs.3.html */
void getIP() {

	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[NI_MAXHOST];
	//char localIP[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	// Walk through linked list, maintaining head pointer so we can free list later

	for (ifa = ifaddr; ifa != NULL ; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL )
			continue;

		family = ifa->ifa_addr->sa_family;

		// For an AF_INET interface address, get the address

		if (family == AF_INET) {
			s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
					NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0) {
				printf("getnameinfo() failed in getIP() function call : %s\n",
						gai_strerror(s));
				//print_errno();
				//exit(EXIT_FAILURE);
			}

			if (strcmp(ifa->ifa_name, "lo")) //ignoring localhost ip address such as 127.0.0.1
				strcpy(localIp, host);

		}
	}

	//freeifaddrs(ifaddr); TODO
}

/* Funtion inserts into the linked list while maintaining the order on the basis of port number.
 code taken from stack overflow and modified according to our needs*/
void insertSortedLinkedList(struct LinkedList *l, struct ListNode *node) {
	struct ListNode *cur, *newNode;

	newNode = malloc(sizeof(struct ListNode)); // create the node to be inserted

	node->next = NULL;
	newNode->isOnline = node->isOnline;
	newNode->sockfd = node->sockfd;
	newNode->port = node->port;
	strcpy(newNode->myip, node->myip);
	strcpy(newNode->myhostname, node->myhostname);

	newNode->sent = node->sent;
	newNode->recieved = node->recieved;
	newNode->storedmsgcount = node->storedmsgcount;

	int z;
	for (z = 0; z < 5; z++) {
		newNode->blockednodes[z] = malloc((25) * sizeof(char));
		strcpy(newNode->blockednodes[z], node->blockednodes[z]);

	}

	if (l->head == NULL ) {
		// linkedlist is empty, inserting as first element
		l->head = newNode;
		l->size++;
		return;
	}

	if (newNode->port <= l->head->port) {
		newNode->next = l->head;
		l->head = newNode;
		l->size++;

		return;
	}

	for (cur = l->head;; cur = cur->next) // loop through the linkedlist
			{
		if (!strcmp(newNode->myip, cur->myip) && cur->port == newNode->port) {
			// element already in the list
			free(newNode);
			return;
		}

		if (!cur->next || cur->next->port >= newNode->port) {
			// next element is bigger than data or end of list, we will insert it now.
			//if (strcmp(newNode->myip, cur->myip)) {
			newNode->next = cur->next;
			cur->next = newNode;
			l->size++;
			//}
			return;
		}
	}
}

//stores message in client buffer and updates num of bytes
void storeMessageInBuffer(struct LinkedList *l, char *srcip, char *msg) {
	struct ListNode *temp;
	temp = l->head;
	int counter = 0;

	while (temp != NULL ) {
		if (!strcmp(srcip, temp->myip))   // && (temp->port==port)
				{
			counter = temp->storedmsgcount;
			temp->message[counter] = malloc(300);
			strcpy(temp->message[counter], msg);
			temp->storedmsgcount = temp->storedmsgcount + 1;
		}
		temp = temp->next;
	}
}

/*@Funtion deleted an entry from linked list maintained by server*/
void deleteNodeFromList(struct LinkedList *l, char myip[], int port) //code taken from stackoverflow
{
	struct ListNode *cur, *prev;

	cur = malloc(sizeof(struct ListNode));
	cur = l->head;
	prev = l->head;

	if (l->head == NULL ) {
		perror("Delete Node Request for Empty List");
		return;
	}

	if (cur->next == NULL ) //head to be deleted
	{
		if (!strcmp(cur->myip, myip) && (cur->port == port)) {
			l->head = cur->next;
			//free(cur);
			l->size--;
			return;
		}
	}

	else {
		while (cur->next != NULL ) { // loop through the linkedlist

			if (!(cur->myip, myip) && (cur->port == port)) //we got the node to delete
					{
				prev->next = cur->next;
				// element already in the list
				//free(cur);
				l->size--;
				return;
			}
			prev = cur;
			cur = cur->next;
		}		    //End of while
	}		    //End of Else
				//return;
}

/* Function changes online status of a client with port number*/
void loginclient(struct LinkedList *l, char ipaddress[], int port) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(ipaddress, temp->myip) && (temp->port == port))
			temp->isOnline = true;
		//printf("%d",temp->port);
		temp = temp->next;
	}
}

void logoutclient(struct LinkedList *l, char ipaddress[], int port) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(ipaddress, temp->myip) && (temp->port == port))
			temp->isOnline = false;
		//printf("%d",temp->port);
		temp = temp->next;
	}
}

bool userHasPendingMessages(struct LinkedList *l, char ipaddress[]) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(ipaddress, temp->myip) && (temp->storedmsgcount > 0))
			return true;
		temp = temp->next;
	}
	return false;
}



void sendAllMessages(struct LinkedList *l, char ipaddress[], int fd) {
	struct ListNode *temp;

	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(ipaddress, temp->myip) && (temp->storedmsgcount > 0)) {
			char msg[300];
			/*char Iplength[10];

			 sprintf(Iplength, "%d",
			 (int)strlen(argument1));

			 strcpy(msg, "BUFA");
			 strcat(msg, Iplength);
			 strcat(msg, ":");
			 strcat(msg, argument1);*/
			int j = 0;
			for (j = 0; j < temp->storedmsgcount; j++) {

				strcpy(msg, temp->message[j]);
				if (send(fd, msg, strlen(msg), 0) == -1) {
					perror("send");
				}

			}				//End of For

			temp->recieved = temp->storedmsgcount; //increase recieved
			temp->storedmsgcount = 0; //reset value
		}

		temp = temp->next;
	} //End of while
}

/* Funtion prints the linked list for LIST command*/
void printList(struct LinkedList *l) {
	struct ListNode *temp;
	temp = l->head;
	int listid = 1;    // for printing list_id
	//printf("came till here\n");
	while (temp != NULL ) {
		if (temp->isOnline) {
			printf("%-5d%-35s%-20s%-8d\n", listid, temp->myhostname, temp->myip,
					temp->port);
			//printf("%d",temp->port);
			listid++; // increment listid
		}
		temp = temp->next;
	}
}

//Function return for checking PrintBlockedList in client machine, where port can be given as an extra arguement
int givePortForIP(struct LinkedList *l, char *str) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(temp->myip, str))
			return temp->port;
		temp = temp->next;
	}
	return -1;
}

/* Function prints the linked list for BLOCKED command*/
void printBlockedList(struct LinkedList *l, char *str, int port) {
	struct ListNode *temp;
	struct ListNode *iterator;
	temp = l->head;
	int listid = 1; int i;   // for printing list_id
	while (temp != NULL ) {
		if (!strcmp(temp->myip, str) && temp->port==port ) { //we got the node
			while(iterator!=NULL){

				for(i=0;i<5;i++)
					if (!strcmp(iterator->myip, temp->blockednodes[i])){
						printf("%-5d%-35s%-20s%-8d\n", listid,iterator->myhostname, iterator->myip,iterator->port);
												listid++;
					}
				iterator = iterator->next;
			}// End of inner while
	}
	temp = temp->next;
  }
}

/* Funtion prints the linked list for STATISTICS command from server*/
void printListStats(struct LinkedList *l) {
	char status[10];
	struct ListNode *temp;
	temp = l->head;
	int listid = 1;    // for printing list_id

	while (temp != NULL ) {
		if (temp->isOnline)
			strcpy(status, "online");
		else
			strcpy(status, "offline");
		printf("%-5d%-35s%-8d%-8d%-8s\n", listid, temp->myhostname, temp->sent,
				temp->recieved, status);
		temp = temp->next;
		listid++; // increment listid
	}
}

/* Function returns a msg that can be sent to server based on "send" or "broadcast"*/
void tokenizeString(char *usercommand, char **msg) {
	char *charray[3];
	*msg = malloc(300);
	char length[10];
	int size = 0;

	char newstr[1024];
	strcpy(newstr, "");

	if (strstr(usercommand, "SEND") != NULL ) {
		charray[0] = strtok(usercommand, " ");
		charray[1] = strtok(NULL, " ");
		charray[2] = strtok(NULL, "\0");
		//charray[2] = usercommand;

		size = strlen(charray[1]);
		size = size + strlen(charray[2]);
		sprintf(length, "%d", size + 1);

		strcat(newstr, "SEND");
		strcat(newstr, length);
		strcat(newstr, ":");
		strcat(newstr, charray[1]);
		strcat(newstr, ",");
		strcat(newstr, charray[2]);
	}

	else if (strstr(usercommand, "BROADCAST") != NULL ) {
		charray[0] = strtok(usercommand, " ");
		charray[1] = strtok(NULL, "\0");
		size = strlen(charray[1]);
		sprintf(length, "%d", size);
		strcat(newstr, "BROD");
		strcat(newstr, length);
		strcat(newstr, ":");
		strcat(newstr, charray[1]);
	}

	strcpy(*msg, newstr);
	strcpy(newstr, ""); //clearing new string

}

///*@funtion converts the List maintained by server into a string*/
/* Return data of ONLINE clients only*/
void giveListAsString(struct LinkedList *l, char **str) {
	struct ListNode *temp;
	temp = l->head;

	char newstr[1024];
	strcpy(newstr, "");
	char charPort[5];

	strcat(newstr, "");
	char* returnval = strdup("");
	while (temp != NULL ) {
		if (temp->isOnline) {
			char* oldval = returnval;
			char * newval;
			printf("print f valueeeee isss %s--%s,----%s,----%d,\n", returnval,
					temp->myhostname, temp->myip, temp->port);
			asprintf(&newval, "%s%s,%s,%d,", returnval, temp->myhostname,
					temp->myip, temp->port);
			free(oldval);
			returnval = newval;

			// strcat(newstr, temp->myhostname);
			//strcat(newstr, ",");
			//strcat(newstr, temp->myip);
			//strcat(newstr, ",");
			//sprintf(charPort, "%d", temp->port);
			//strcat(newstr, charPort);
			//printf("%d",temp->port);
			// strcat(newstr, ",");
		}
		temp = temp->next;
	}

	*str = returnval;
}

/* prints message with source ip in client machine*/
void printStoredMessage(char *str) {

	int counter = 0;
	char *charray[2];

	charray[counter] = strtok(str, ",");
	counter++;
	charray[counter] = "";

	while (charray[counter] != NULL && counter < 2) {

		charray[counter] = strtok(NULL, ",");
		counter++;
		if (counter == 2 && charray[1] != NULL ) {
			cse4589_print_and_log("[%s:SUCCESS]\n", "RECEIVED");
			cse4589_print_and_log("msg from:%s\n[msg]:%s", charray[0],
					charray[1]);
			cse4589_print_and_log("[%s:END]\n", "RECEIVED");
			fflush(stdout);

			counter = 0;
			charray[counter++] = strtok(NULL, ",");
			if (charray[0] == NULL ) {
				break;
			}
		}

	}	// END while
}

/*@funtion prints the string received from server required in LIST format*/
void printStringInFormat(char *str) {

	int counter = 0;
	int listid = 1;

	char *charray[3];

	/* get the first token*/
	charray[counter] = strtok(str, ",");
	counter++;
	charray[counter] = "";

	/* walk through other tokens */
	while (charray[counter] != NULL && counter < 3) {

		charray[counter] = strtok(NULL, ",");
		counter++;
		if (counter == 3 && charray[2] != NULL ) {
			cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", listid, charray[0],
					charray[1], atoi(charray[2]));
			fflush(stdout);

			counter = 0;
			charray[counter++] = strtok(NULL, ",");
			if (charray[0] == NULL ) {
				break;
			}
			listid++;
		}

	}	// END while

	//printf("\n At function : formatstring end value is str: %s\t temp %s:\n",str,temp);
	//fflush(stdout);
}

void splitMessage(char *str, char *details[]) {

	details[0] = strtok(str, ",");
	details[1] = strtok(NULL, "\0");
}

//taken from http://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c/
bool isValidIpAddress(char *ipAddress) {
	struct sockaddr_in sa;
	int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr)); // returns 0 if invalid IP, returns 1 on success
	if (result == 0)
		return false;
	else
		return true;
}

bool isValidPort(int port) {

	if (port > 0 && port < 65536)
		return true;
	else
		return false;
}

/*@funtion prints the string received from server required in LIST format*/
void fillPeerAddress(char *str, char *IpAdress[]) {

	int counter = 0;
	int ipcounter = 0;
	int listid = 1;

	char *charray[3];

	//printf("incoming string is %s\n", str);

	/* get the first token*/
	charray[counter] = strtok(str, ",");
	counter++;
	charray[counter] = "";

	/* walk through other tokens */
	while (charray[counter] != NULL && counter < 3) {
		charray[counter] = strtok(NULL, ",");
		counter++;
		//printf("charray[1] is %s\n", charray[counter - 1]);

		if (counter == 3 && charray[2] != NULL ) {
			//printf("inside if     charray[1] is %s\n", charray[1]);
			IpAdress[ipcounter] = charray[1]; //check this line
			counter = 0;
			charray[counter++] = strtok(NULL, ",");
			if (charray[0] == NULL ) {
				break;
			}
			listid++;
			ipcounter++;
		}

	}		// END while
}		// END of function

void incrementReceived(struct LinkedList *l, char dest[], int port) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(dest, temp->myip) && (temp->port == port))
			temp->recieved = temp->recieved + 1;
		//printf("%d",temp->port);
		temp = temp->next;
	}
}

void incrementSent(struct LinkedList *l, char src[], int port) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {
		if (!strcmp(src, temp->myip) && (temp->port == port))
			temp->sent = temp->sent + 1;
		//printf("%d",temp->port);
		temp = temp->next;
	}
}

/*Function returns true if the dest ip has blocked src ip, false otherwise. Increments message sent value*/
bool isBlocked(struct LinkedList *l, char src[], char dest[], int port) {
	struct ListNode *temp;
	temp = l->head;

	int i;
	while (temp != NULL ) {
		if (!strcmp(dest, temp->myip) && (temp->port == port)) //get the node of the source client
				{
			for (i = 0; i < 5; i++) {
				if (!strcmp(temp->blockednodes[i], src)) {
					return true;
				}
			}

		}
		temp = temp->next;
	}
	return false;
}

/*Function returns true if the given IP is online, false otherwise.
 It also sets the socket descriptor, if its online*/

bool isOnline(struct LinkedList *l, char dest[], int port, int *fd) {
	struct ListNode *temp;
	temp = l->head;

	while (temp != NULL ) {

		char *status; //just for printing
		if (temp->isOnline)
			status = "Online";
		else
			status = "Offline";
		printf("values in in isline function %s\t%d\tisOnline bool value:%s\n",
				temp->myip, temp->port, status);

		if (!strcmp(dest, temp->myip) && (temp->port == port)) { //get the node of the source client using IP and POrt
			if (temp->isOnline) {
				*fd = temp->sockfd;    //giving back sock fd for sending message
				return true;
			} else
				return false;

		}
		temp = temp->next;

	}
	return false;
}

/*Function adds destination IP into the source IP's blocked list*/
void blockThisCLient(struct LinkedList *l, char src[], char dest[], int port) {
	struct ListNode *temp;
	temp = l->head;

	int i;
	while (temp != NULL ) {
		if (!strcmp(src, temp->myip) && (temp->port == port)) { //get the node of the source client
			//traverse through the blocked array
			for (i = 0; i < 5; i++) {
				if (!strcmp(temp->blockednodes[i], "a"))
				{
					strcpy(temp->blockednodes[i], dest);
					break;    //without break, rewrites all the 5 elements
				}
				//temp->blockednodes[i] = dest;
			}
		}
		temp = temp->next;

	}
}				//END of function

/*Function removes destination IP into the source IP's blocked list*/
void unBlockThisCLient(struct LinkedList *l, char src[], char dest[], int port) {
	struct ListNode *temp;
	temp = l->head;

	int i;
	while (temp != NULL ) {
		if (!strcmp(src, temp->myip) && (temp->port == port)) { //get the node of the source client
			//traverse through the blocked array
			for (i = 0; i < 5; i++) {
				if (!strcmp(temp->blockednodes[i], dest))
					strcpy(temp->blockednodes[i], "a");
				//temp->blockednodes[i] = dest;
			}
		}
		temp = temp->next;
	}
}

bool isUserReturning(struct LinkedList *l, char ip[], int port) {
	struct ListNode *temp;
	temp = l->head;

	int i;
	while (temp != NULL ) {
		if (!strcmp(ip, temp->myip)) { //get the node of the source client

			if (temp->port == port)
				return true;
		}

		temp = temp->next;
	}

	return false;
}

int readLength(int sockfd) {

	int nbytes = 0;
	char len[10];
	char singlechar;
	int i = 0;
	int length;

	do {
		if ((nbytes = recv(sockfd, &singlechar, 1, 0)) <= 0) {
			perror("address read error");
			return -1;
		}
		len[i] = singlechar;
		i++;
	} while (singlechar != ':');

	len[i - 1] = '\0';

	length = atoi(len);

	return length;
}

void receiveBytes(int length, int sockfd, char *str) {
	char singlechar;
	int nbytes = 0;
	int bytesrecv = 0;
	//char len[length];
	int i = 0;

	do {
		if ((nbytes = recv(sockfd, &singlechar, 1, 0)) <= 0)
			perror("receive bytes error :server");
		str[i] = singlechar;
		//printf("character in receive bytes is %c\n", singlechar);
		i++;
		bytesrecv = bytesrecv + nbytes;

	} while (bytesrecv < length);

	str[i] = '\0';

	printf("complete String coming out of receive bytes is  %s\n", str);
	//printf("bytes received is %d\n", bytesrecv);
}

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */

int main(int argc, char **argv) {
	/*Init. Logger*/
	cse4589_init_log(argv[2]);

	/*Clear LOGFILE*/
	fclose(fopen(LOGFILE, "w"));

	/*Start Here*/
	printf("I agree to the terms and conditions\n");

	if (argc != 2) {
		printf("Verify Command Line Input: Port Number\n");
	}

	char *app = argv[1];
	char *port = argv[2]; // for using in getaddrinfo

#define PORT atoi(argv[2])  //for integer type of port
	if (strcmp(app, "s") == 0)  //Run as Server application
			{

		//Below Code template taken from Beej guide

		fd_set master;    // master file descriptor list
		fd_set read_fds;  // temp file descriptor list for select()
		int fdmax;        // maximum file descriptor number

		int listener;     // listening socket descriptor
		int newfd;        // newly accept()ed socket descriptor
		struct sockaddr_storage remoteaddr; // client address
		socklen_t addrlen;

		char buf[256];    // buffer for client data
		int nbytes;
		char command[30];
		char userCommand[30];
		char argument1[256];  //these arguments could be a message
		char argument2[256];

		char clientIp[128];  //extract data from command

		struct LinkedList mylinkedList; //linkedlist of server for maintaining client values
		mylinkedList.head = NULL;
		mylinkedList.size = 0;

		char *listToStr; //string used to store linked list data

		char remoteIP[INET6_ADDRSTRLEN];

		int yes = 1;        // for setsockopt() SO_REUSEADDR, below
		int i, j, rv;

		struct addrinfo hints, *ai, *p;

		FD_ZERO(&master);
		// clear the master and temp sets
		FD_ZERO(&read_fds);

		// get us a socket and bind it
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;  //fill in my IP for me
		if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
			fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
			//exit(1);
		}

		for (p = ai; p != NULL ; p = p->ai_next) {

			listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
			if (listener < 0) {
				continue;
			}

			// lose the pesky "address already in use" error message
			if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof(int)) == -1) {
				perror("setsockopt");
				continue;
			}

			//before binding we need the right address.

			if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
				close(listener);
				continue;
			}

			break;
		}

		// if we got here, it means we didn't get bound
		if (p == NULL ) {
			fprintf(stderr, "selectserver: failed to bind\n");
			//exit(2);
		}

		freeaddrinfo(ai); // all done with this

		// listen with maximum of 5 connections
		if (listen(listener, 5) == -1) {
			perror("listen");
			//exit(3);
		}

		// adding stdin and listener fd's to the master set
		FD_SET(STDIN, &master);
		FD_SET(listener, &master);

		// keep track of the biggest file descriptor
		fdmax = listener; // so far, it's this one

		printf("server is listening on... %d \n", PORT);
		fflush(stdout);

		// main loop
		for (;;) {
			read_fds = master; // copy it
			printf("TYPE YOUR COMMAND HERE > ");
			fflush(stdout);
			if (select(fdmax + 1, &read_fds, NULL, NULL, NULL ) == -1) {
				perror("select");
				exit(4);
			}

			// run through the existing connections looking for data to read
			for (i = 0; i <= fdmax; i++) {

				if (FD_ISSET(i, &read_fds)) { // we got one!!

					if (i == STDIN) {
						// handle data from your own STDIN, commands like AUTHOR, IP etc.

						fgets(userCommand, sizeof(userCommand), stdin); //to support space characters
						sscanf(userCommand, "%s %s %s", command, argument1,
								argument2);

						if (!strcmp("AUTHOR", command)) {
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log(
									"I, %s, have read and understood the course academic integrity policy.\n",
									ubit_name);
							cse4589_print_and_log("[%s:END]\n", command);
						}

						else if (!strcmp("IP", command)) {
							getIP();  //sets value for localIp address
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("IP:%s\n", localIp);
							cse4589_print_and_log("[%s:END]\n", command);
						}

						else if (!strcmp("LIST", command)) {
							getIP();  //sets value for localIp address
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							printList(&mylinkedList);
							cse4589_print_and_log("[%s:END]\n", command);
						}

						else if (!strcmp("PORT", command)) {
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("PORT:%d\n", PORT);
							cse4589_print_and_log("[%s:END]\n", command);

						}

						else if (!strcmp("STATISTICS", command)) {
							cse4589_print_and_log("[%s:SUCCESS]\n", command);

							printListStats(&mylinkedList);

							cse4589_print_and_log("[%s:END]\n", command);

						}

						else if (!strcmp("BLOCKED", command)) {

							if(argument1!=NULL)
							{
							cse4589_print_and_log("[%s:SUCCESS]\n", command);

							printBlockedList(&mylinkedList, argument1,givePortForIP(&mylinkedList,argument1));

							cse4589_print_and_log("[%s:END]\n", command);
							}
							else
							{
								cse4589_print_and_log("[%s:ERROR]\n", command);
								cse4589_print_and_log("[%s:END]\n", command);
							}
						}

						else if (!strcmp("EXIT", command)) {

							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							//printListStats(&mylinkedList);
							cse4589_print_and_log("[%s:END]\n", command);
							exit(0);
						}

					}

					else if (i == listener) {
						// handle new connections
						addrlen = sizeof remoteaddr;
						newfd = accept(listener,
								(struct sockaddr *) &remoteaddr, &addrlen);

						if (newfd == -1) {
							perror("accept");
						} else {
							FD_SET(newfd, &master);
							// add to master set
							if (newfd > fdmax) {    // keep track of the max
								fdmax = newfd;
							}
							printf("selectserver: new connection from %s on "
									"socket %d\n",
									inet_ntop(remoteaddr.ss_family,
											get_in_addr(
													(struct sockaddr*) &remoteaddr),
											remoteIP, INET6_ADDRSTRLEN), newfd); //ipaddress of the client

						}
					} else {
						// handle data from a client
						if ((nbytes = recv(i, buf, 4, 0)) <= 0) {

							if (nbytes == 0) {
								// connection closed
								printf("selectserver: socket %d hung up\n", i);
							} else if (nbytes != 4) {
								perror("recv");
							}

							close(i); // bye!
							FD_CLR(i, &master);
							// remove from master set

						} else {
							// we got some data from a client, recv() call is success

							//printf("command is ########%s######\n", buf);

							buf[nbytes] = '\0';

							struct sockaddr_in clientsock;
							int clientsock_len;
							clientsock_len = sizeof(clientsock);

							if (getpeername(i, (struct sockaddr*) &clientsock,
									&clientsock_len) == -1) {
								perror("getpeername() failed");
								return -1;
							}

							char hostName[NI_MAXHOST];

							strcpy(remoteIP, inet_ntoa(clientsock.sin_addr));
							//int truePort = ntohs()

							//Finding HostName from IP
							clientsock.sin_family = AF_INET;
							inet_pton(AF_INET, remoteIP, &clientsock.sin_addr);
							getnameinfo((struct sockaddr *) &clientsock,
									sizeof(clientsock), hostName,
									sizeof(hostName), NULL, 0, 0);

							//printf("hostName name is %s",hostName);

							if (strstr(buf, "LOGO") != NULL ) {

								int sockfd = i;
								int length = readLength(sockfd);
								//printf("length is %d\n", length);

								char msg[length];
								receiveBytes(length, sockfd, msg); //receive all bytes

								int truePort = givePortForIP(&mylinkedList,
										remoteIP);

								logoutclient(&mylinkedList, remoteIP, truePort); //change status
								//close(i);  //closing sockfd
							}

							else if (strstr(buf, "LOGI") != NULL ) //LOGIN LOGIN LOGIN
							{
								//loginclient(&mylinkedList,remoteIP); //change status

								int sockfd = i;
								int length = readLength(sockfd);

								printf("length of message is %d\n", length);
								fflush(stdout);

								if (length == -1)
									printf("did not read enough bytes in login\n");

								char msg[length];

								receiveBytes(length, sockfd, msg);

								printf(" message is %s\n", msg);
								fflush(stdout);

								//create object and insert only if new user, else change status and send LIST
								if (!isUserReturning(&mylinkedList, remoteIP,
										atoi(msg))) //if first time user, set default values
												{

									struct ListNode obj1;
									obj1.sockfd = i;
									obj1.port = atoi(msg);
									strcpy(obj1.myip, remoteIP);
									strcpy(obj1.myhostname, hostName);
									obj1.isOnline = true;

									obj1.sent = 0;
									obj1.recieved = 0;
									obj1.storedmsgcount = 0;

									int z;
									for (z = 0; z < 5; z++) {
										obj1.blockednodes[z] = malloc(
												(25) * sizeof(char));
										strcpy(obj1.blockednodes[z], "a");

									}

									insertSortedLinkedList(&mylinkedList,
											&obj1); //Add this object client detail to list

								}

								if (userHasPendingMessages(&mylinkedList,
										remoteIP)) {

									sendAllMessages(&mylinkedList,remoteIP,sockfd);
									//Send him all the messages and change his received count.

								}

								loginclient(&mylinkedList, remoteIP, atoi(msg));

								giveListAsString(&mylinkedList, &listToStr);

								char message[50];
								char listlength[10];

								sprintf(listlength, "%d",
										(int) strlen(listToStr));

								strcpy(message, "LIST");
								strcat(message, listlength);
								strcat(message, ":");
								strcat(message, listToStr);

								printf("\nstring sent to client now is %s \n",
										message);
								fflush(stdout);

								if (send(newfd, message, strlen(message), 0)
										== -1)
									perror("send");
								//close(i);  //closing sockfd
							}

							else if (strstr(buf, "EXIT") != NULL ) {

								int sockfd = i;
								int length = readLength(sockfd);
								printf("length is %d\n", length);

								char msg[length];
								receiveBytes(length, sockfd, msg); //receive all bytes

								int truePort = givePortForIP(&mylinkedList,
										remoteIP);

								printf("Reaches HEREE :EXIT\n");

								deleteNodeFromList(&mylinkedList, remoteIP,
										truePort); //delete client information from list

								close(i);  //closing sockfd  remove it from master set
								FD_CLR(i, &master);
							}

							else if (strstr(buf, "SEND") != NULL ) {

								int sockfd = i;
								int length = readLength(sockfd);
								printf("length is %d\n", length);

								char msg[length];
								receiveBytes(length, sockfd, msg);

								printf("msg got is ####%s####\n", msg);

								//message is the original message given by client
								char *detail[2];

								int fd;
								splitMessage(strdup(msg), detail);

								printf(
										"\nreturned values from split message function %s\t##%s###\n",
										detail[0], detail[1]);

								char message[300];
								char msglength[10];

								int mlength = strlen(detail[0])
										+ strlen(detail[1]) + 1; //+1 for comma added

								sprintf(msglength, "%d", mlength);

								strcpy(message, "MESG");
								strcat(message, msglength);
								strcat(message, ":");
								strcat(message, detail[0]);
								strcat(message, ",");
								strcat(message, detail[1]);

								int truePort = givePortForIP(&mylinkedList,
										detail[0]);

								//whether remote Ip is blocked by the person he wants to send
								if (!isBlocked(&mylinkedList, remoteIP,
										detail[0], truePort)) {

									printf("\nUSer is not blocked\n");

									if (truePort == -1)
										printf(
												"!!!!Port number is not returned in SEND Line:1112"); //gives first port number in local machine

									if (isOnline(&mylinkedList, detail[0], //detail[0] has IP, detail[1] has message
											truePort, &fd)) { //this function puts message into client buffer as well.
										printf(
												"\nUser is not blocked and is online\n");
										fflush(stdout);

										if (send(fd, message, strlen(message),
												0) > 0) { //send message successful
											cse4589_print_and_log(
													"[%s:SUCCESS]\n",
													"RELAYED");
											cse4589_print_and_log(
													"msg from:%s, to:%s\n[msg]:%s\n",
													remoteIP, detail[0],
													detail[1]);
											cse4589_print_and_log("[%s:END]\n",
													"RELAYED");

											int senderPort = givePortForIP(
													&mylinkedList, remoteIP);

											incrementSent(&mylinkedList, //Increment both sender and receiver counter
													remoteIP, senderPort);

											incrementReceived(&mylinkedList,
													detail[0], truePort); //

										}
									} // User not Online

									storeMessageInBuffer(&mylinkedList,
											detail[0], message);

								} //user blocked

								//incrementReceived(&mylinkedList, detail[0]); //increment when the client logs in
							}

							if (strstr(buf, "BLOC") != NULL ) {

								int sockfd = i;

								int length = readLength(sockfd);
								char msg[length];
								receiveBytes(length, sockfd, msg);

								printf(
										"\nCame into blocked Function of server\n");
								fflush(stdout);

								int truePort = givePortForIP(&mylinkedList,
										remoteIP);

								blockThisCLient(&mylinkedList, remoteIP, msg,
										truePort); //msg is nothing but the IP to be blocked
							}

							if (strstr(buf, "UNBL") != NULL ) {
								int sockfd = i;
								int length = readLength(sockfd);
								char msg[length];

								receiveBytes(length, sockfd, msg);

								int truePort = givePortForIP(&mylinkedList,
										remoteIP);

								unBlockThisCLient(&mylinkedList, remoteIP, msg,
										truePort);
							}

							if (strstr(buf, "BROD") != NULL ) {

								int sockfd = i;
								int length = readLength(sockfd);
								char msg[length];

								receiveBytes(length, sockfd, msg);

								printf("the message received is %s\n", msg);

								// send to everyone!
								for (j = 0; j <= fdmax; j++) {

									if (FD_ISSET(j, &master)) {

										//char *detail[2];
										int fd; //for returning fd's of other clients
										//splitMessage(strdup(msg), detail);

										if (j != i && j != listener) { //send the date back to client
										//1.find Ip address using this fd, 2. check if remoteip is blocked, 3. isonline, 4.send

											socklen_t len;
											struct sockaddr_storage addr;
											char peerIP[INET6_ADDRSTRLEN];

											len = sizeof addr;
											getpeername(j,
													(struct sockaddr*) &addr,
													&len);

											struct sockaddr_in *s =
													(struct sockaddr_in *) &addr; //code taken from satckoverflow
											inet_ntop(AF_INET, &s->sin_addr,
													peerIP, sizeof peerIP);

											int Boradcastport = givePortForIP(
													&mylinkedList, peerIP);

											char message[300];
											char msglength[10];

											int length = strlen(remoteIP)
													+ strlen(msg) + 1; //+1 for comma added

											sprintf(msglength, "%d", length);

											strcpy(message, "MESG");
											strcat(message, msglength);
											strcat(message, ":");
											strcat(message, remoteIP);
											strcat(message, ",");
											strcat(message, msg);

											//whether remote Ip has been blocked by person he wants to send
											if (!isBlocked(&mylinkedList,
													remoteIP, peerIP,
													Boradcastport)) {
												if (isOnline(&mylinkedList,
														peerIP, //detail[0] has IP, detail[1] has message
														Boradcastport, &fd)) { //this function gives fd, puts message into client buffer if offline, .

													if (send(fd, message,
															strlen(message), 0)
															> 0)
														printf(
																"\n message: %s, from %s is sent to everyone\n",
																message,
																remoteIP);

													incrementReceived(
															&mylinkedList,
															peerIP,
															Boradcastport); //msg successfully delivered

												} // not online

												storeMessageInBuffer(
														&mylinkedList, peerIP,
														message);

											} //blocked user
										}

									} // END of sending data to clients

								}

								cse4589_print_and_log("[%s:SUCCESS]\n",
										"RELAYED");
								cse4589_print_and_log(
										"msg from:%s, to:%s\n[msg]:%s\n",
										remoteIP, "255.255.255.255", msg);
								cse4589_print_and_log("[%s:END]\n", "RELAYED");

								int truePort = givePortForIP(&mylinkedList,
										remoteIP);

								incrementSent(&mylinkedList, remoteIP,
										truePort); //msg is sent

							}

							if (!strcmp(buf, "REFR")) {

								listToStr = ""; //refresh list string and then fill it

								//printList(&mylinkedList); //printing in server side for reference

								giveListAsString(&mylinkedList, &listToStr);

								char message[200];
								char listlength[10];

								sprintf(listlength, "%d",
										(int) strlen(listToStr));

								strcpy(message, "LIST");
								strcat(message, listlength);
								strcat(message, ":");
								strcat(message, listToStr);

								printf(
										"\nfresh list sent to client now is %s \n",
										message);

								if (send(i, message, strlen(message), 0) == -1)
									perror("send");
							}

							//printf("data sent to client is %s\n", buf); //print data from client

						}
						//if(!strcmp(buf,"LOGIN"))
						//first receive client details, ip,port and hostname add to list.
						//add client details to server linked list
						//then send the list to cliet as a string

						//code for broadcast
						/* for(j = 0; j <= fdmax; j++) {
						 // send to everyone!
						 if (FD_ISSET(j, &master)) {
						 // except the listener and ourselves
						 if (j != i) {   //send the date back to client
						 if (send(j, buf, nbytes, 0) == -1) {
						 perror("send");
						 }
						 }
						 }
						 }*/

					} // END handle data from client
				} // END got new incoming connection
			} // END looping through file descriptors
		} // END for(;;)--and you thought it would never end!

	} //End of if loop server code

	else if (strcmp(app, "c") == 0)  //else loop for client process

			{

// Below code template taken from Beej guide for client process

		int sockfd = -1, numbytes;
		char clientbuf[MAXDATASIZE];
		struct addrinfo hints, *servinfo, *p;
		int rv, i;
		char s[INET6_ADDRSTRLEN];
		char *temp;
		fd_set masterclient;    // master file descriptor list
		fd_set read_fds;  // temp file descriptor list for select()
		int fdmax;        // maximum file descriptor number

		char buf[1024];    // buffer for client data
		int nbytes;
		char command[30];
		char userCommand[1024];
		char argument1[256];  //these arguments could be a message
		char argument2[256];
		char *listString;
		char *msgToServer;
		char liststringBackup[256];
		char *peerIpAdress[5];  //Array for storing Ip adresses from the list
		char *blockedIpAddress[5];  // = malloc(5 * sizeof(char*));

		int x;
		for (x = 0; x < 5; x++) {
			peerIpAdress[x] = malloc((20) * sizeof(char));
		}

		int y;
		for (y = 0; y < 5; y++) {
			blockedIpAddress[y] = malloc((20) * sizeof(char));
			strcpy(blockedIpAddress[y], "a");
		}

		bool loggedIn = false; //condition variable for client log in

		char serverIp[INET6_ADDRSTRLEN]; //server information obtained from LOGIN command
		char serverPort[4];
		char clientIp[INET6_ADDRSTRLEN];

		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		// add the listener to the master set
		FD_SET(STDIN, &masterclient);
//FD_SET(sockfd, &masterclient);

// keep track of the biggest file descriptor
		fdmax = STDIN; // so far, it's this one , how sockfd would come here??

		printf("client is ready to connect");
		fflush(stdout);

		// main loop
		for (;;) {

			read_fds = masterclient; // copy it
			printf("TYPE YOUR COMMAND HERE > ");
			fflush(stdout);
			if (select(fdmax + 1, &read_fds, NULL, NULL, NULL ) == -1) {
				perror("select");
				//exit(4);
			}

			//run through the existing connections looking for data to read
			for (i = 0; i <= fdmax; i++) {
				if (FD_ISSET(i, &read_fds)) { // we got one!!

					//printf("socket id %d\n", i);

					if (i == STDIN) {
						// handle data from your own STDIN, commands like AUTHOR, IP etc.

						fflush(stdout);
						fgets(userCommand, sizeof(userCommand), stdin); //to support space characters
						userCommand[strlen(userCommand) - 1] = '\0';

						sscanf(userCommand, "%s %s %s", command, argument1,
								argument2);

						if (!strcmp(command, "AUTHOR")) {
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log(
									"I, %s, have read and understood the course academic integrity policy.\n",
									ubit_name);
							cse4589_print_and_log("[%s:END]\n", command);
						}

						else if (strstr(command, "LOGIN") != NULL ) { //if command contains word 'LOGIN'

							//printf("coming inside, IP is kk%skk and kk%sjj\n",serverIp,serverPort);
							//fflush(stdout);
							strcpy(serverIp, argument1);
							strcpy(serverPort, argument2);

							bool validConnection = true;

							if (isValidIpAddress(argument1)
									&& isValidPort(atoi(argument2))) {

								if (sockfd == -1) {
									//Connect to the server
									if ((rv = getaddrinfo(serverIp, serverPort,
											&hints, &servinfo)) != 0) {
										fprintf(stderr, "getaddrinfo: %s\n",
												gai_strerror(rv));
										return 1;
									}

									for (p = servinfo; p != NULL ;
											p = p->ai_next) {

										if ((sockfd = socket(p->ai_family,
												p->ai_socktype, p->ai_protocol))
												== -1) {
											perror("client socket creation");
											continue;
										}

										if (connect(sockfd, p->ai_addr,
												p->ai_addrlen) == -1) {
											close(sockfd);
											validConnection = false; //set connection as invalid
											sockfd =-1;
											perror("client connect call");
											continue;
										}
										inet_ntop(p->ai_family,
												get_in_addr(
														(struct sockaddr *) p->ai_addr),
												s, sizeof s);
										printf("client: connecting to %s\n", s);
										fflush(stdout);

										//freeaddrinfo(servinfo); TODO // all done with this structure

									} //end of for loop

									if (validConnection) {
									if (sockfd > fdmax)  //set max descriptor
										fdmax = sockfd;

									FD_SET(sockfd, &masterclient);}
									//add server descriptor to read_fds
								} // END of IF LOOP

								//if(p!=NULL)
								//{

								if (validConnection) {

									loggedIn = true; //client successfully logged into server

									char msg[50];
									char portlength[10];

									sprintf(portlength, "%d",
											(int) strlen(argv[2]));

									strcpy(msg, "LOGI");
									strcat(msg, portlength);
									strcat(msg, ":");
									strcat(msg, argv[2]);

									if (send(sockfd, msg, strlen(msg), 0) == -1) //let server know you performed login with PORT
									{
										perror("send");
										validConnection = false;//user logs in back, with wrong port or Ip (syntactically right)
									}

									if (validConnection) {
									printf(
											"login message sent to server is %s\n",
											msg);

									cse4589_print_and_log("[%s:SUCCESS]\n",
											command);
									cse4589_print_and_log("[%s:END]\n",
											command);
									}
								}

							} // end of valid ip check

							if (!validConnection) {
								cse4589_print_and_log("[%s:ERROR]\n", command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

						}

						else if (!strcmp(command, "IP") && loggedIn) {
							getIP();  //sets value for localIp address
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("IP:%s\n", localIp);
							cse4589_print_and_log("[%s:END]\n", command);
						}

						else if (!strcmp(command, "PORT") && loggedIn) {
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("PORT:%d\n", PORT);
							cse4589_print_and_log("[%s:END]\n", command);

						}

						else if (!strcmp(command, "REFRESH") && loggedIn) {

							char msg[50];
							strcpy(msg, "REFR");
							strcat(msg, "0");
							strcat(msg, ":");
							strcat(msg, "\0");

							if (send(sockfd, msg, strlen(msg), 0) == -1) //let server know you performed login with PORT
									{
								perror("send");
							}

							listString = "";              //Emptying list string
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("[%s:END]\n", command);

						}

						else if (strstr(command, "SEND") != NULL && loggedIn) {

							bool Ipexists = false;

							int z;
							for (z = 0; z < 5; z++) {
								if (!strcmp(peerIpAdress[z], argument1))
									Ipexists = true;
							}

							if (Ipexists && isValidIpAddress(argument1)) {

								tokenizeString(strdup(userCommand),
										&msgToServer); //tokenizes user command, bcs msg can have space.

								/*returned message is of format SEND<MSGLENGTH>:IP,MESSAGE */

								printf(
										"\nMessage returned by tokenize string is %s\n",
										msgToServer);

								if (send(sockfd, msgToServer,
										strlen(msgToServer), 0) == -1) {
									perror("send");
								}

								printf("\n message sent to server is %s\n",
										msgToServer);
								fflush(stdout);

								strcpy(msgToServer, ""); //clearing msg variable

								cse4589_print_and_log("[%s:SUCCESS]\n",
										command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

							else{
								cse4589_print_and_log("[%s:ERROR]\n",command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

						}

						else if (strstr(command, "BROADCAST") != NULL
								&& loggedIn) {

							tokenizeString(strdup(userCommand), &msgToServer); //tokenizes user command, bcs msg can have space.

							/*returned message is of format SEND<MSGLENGTH>:IP,MESSAGE */
							printf(
									"\nMessage returned by tokenize string is %s\n",
									msgToServer);

							if (send(sockfd, msgToServer, strlen(msgToServer),
									0) == -1) {
								perror("send");
							}

							printf("\n message sent to server is %s\n",
									msgToServer);

							strcpy(msgToServer, "");
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("[%s:END]\n", command);

						}

						else if (!strcmp(command, "BLOCK") && loggedIn) {

							bool Ipexists = false;

							int z;
							for (z = 0; z < 5; z++) {
								if (!strcmp(peerIpAdress[z], argument1))
									Ipexists = true;
							}

							if (Ipexists && isValidIpAddress(argument1)) //get in only if ip exists and valid
									{

								char msg[50];
								char Iplength[10];

								sprintf(Iplength, "%d",
										(int) strlen(argument1));

								strcpy(msg, "BLOC");
								strcat(msg, Iplength);
								strcat(msg, ":");
								strcat(msg, argument1);

								bool blocked = false;

								int j;
								for (j = 0; j < 5; j++) {
									if (!strcmp(blockedIpAddress[j], argument1))
										blocked = true;
								}

								if (blocked == false) //value not in array, so put it in our array
										{
									for (j = 0; j < 5; j++)
										if (!strcmp(blockedIpAddress[j], "a"))
											strcpy(blockedIpAddress[j],
													argument1);
								}

								printf("crossed second for loop in block");
								fflush(stdout);

								if (!blocked && isValidIpAddress(argument1)) {
									if (send(sockfd, msg, strlen(msg), 0)
											== -1) {
										perror("send");
									}

									cse4589_print_and_log("[%s:SUCCESS]\n",
											command);
									cse4589_print_and_log("[%s:END]\n",
											command);
								}

							} // END of valid IP check

							else {
								cse4589_print_and_log("[%s:ERROR]\n", command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

						} //END of BLocked

						else if (strstr(command, "UNBLOCK") != NULL
								&& loggedIn) {
							//if(!logout) then logout

							bool Ipexists = false;

							int z;
							for (z = 0; z < 5; z++) {
								if (!strcmp(peerIpAdress[z], argument1))
									Ipexists = true;
							}

							if(Ipexists && isValidIpAddress(argument1))
							{

							char msg[50];
							char Iplength[10];

							sprintf(Iplength, "%d", (int) strlen(argument1));

							strcpy(msg, "UNBL");
							strcat(msg, Iplength);
							strcat(msg, ":");
							strcat(msg, argument1);

							bool blocked = false;

							int j;
							for (j = 0; j < 5; j++) { // check if already blocked
								if (!strcmp(blockedIpAddress[j], argument1))
									blocked = true;
							}

							if (blocked == true) //value is in local array, so remove it
									{
								for (j = 0; j < 5; j++)
									if (!strcmp(blockedIpAddress[j], argument1))
										strcpy(blockedIpAddress[j], "a");
							}

							if (blocked && isValidIpAddress(argument1)) {
								if (send(sockfd, msg, strlen(msg), 0) == -1) {
									perror("send");
								}

								cse4589_print_and_log("[%s:SUCCESS]\n",
										command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

							} //End of valid Ip check
							else {
								cse4589_print_and_log("[%s:ERROR]\n", command);
								cse4589_print_and_log("[%s:END]\n", command);
							}

						}

						//listString
						else if (!strcmp(command, "LIST") && loggedIn) {

							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							//strcpy(liststringBackup, listString);
							printStringInFormat(strdup(listString));
							//strcpy(listString, liststringBackup);
							//strcpy(liststringBackup, "");
							cse4589_print_and_log("[%s:END]\n", command);

							printf("list string is %s\n", listString);
							fflush(stdout);

						}

						else if (!strcmp(command, "EXIT")) {

							char msg[50];
							strcpy(msg, "EXIT");
							strcat(msg, "0");
							strcat(msg, ":");
							strcat(msg, "\0");

							if (send(sockfd, "EXIT", strlen(msg), 0) == -1) {
								perror("send");
							}
							loggedIn = false;

							printf("exiting application... \n");
							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("[%s:END]\n", command);
							exit(0);
						}

						else if (!strcmp(command, "LOGOUT") && loggedIn) {
							//if(!logout) then logout
							char msg[50];
							strcpy(msg, "LOGO");
							strcat(msg, "0");
							strcat(msg, ":");
							strcat(msg, "\0");

							if (send(sockfd, msg, strlen(msg), 0) == -1) //let server know you performed login with PORT
									{
								perror("send");
							}

							loggedIn = false;
							//send msg to server that you logged out
							if (send(sockfd, msg, strlen(msg), 0) == -1) {
								perror("send");
							}

							cse4589_print_and_log("[%s:SUCCESS]\n", command);
							cse4589_print_and_log("[%s:END]\n", command);
							printf("Logging out... \n");
						}

					} //END stdinput

					if (i == sockfd) {

						// handle data from a server
						if ((nbytes = recv(i, buf, 4, 0)) <= 0) {
							// got error or connection closed by client
							if (nbytes == 0) {
								printf("select client: socket %d hung up\n", i);
							} else {
								perror("recv");
							}
							close(i); // bye!
							FD_CLR(i, &masterclient);
// remove from master set
						} else {
// we got some data from a server

							buf[nbytes] = '\0';

							if (!strcmp("LIST", buf)) {
								//listString = buf + 4; //List from server stored in client

								int sockfd = i;
								int length = readLength(sockfd);
								char msg[length];

								receiveBytes(length, sockfd, msg);

								listString = malloc(
										(sizeof(char) * length) + 1);

								strcpy(listString, msg);

								fillPeerAddress(strdup(listString),
										peerIpAdress);
							}

							if (!strcmp("MESG", buf)) {

								int sockfd = i;
								int length = readLength(sockfd);
								char msg[length];

								receiveBytes(length, sockfd, msg);

								char *detail[2];
								splitMessage(strdup(msg), detail); // detail[0] has sender Ip, detail[1] has message

								cse4589_print_and_log("[%s:SUCCESS]\n",
										"RECEIVED");
								cse4589_print_and_log("msg from:%s\n[msg]:%s\n",
										detail[0], detail[1]);
								cse4589_print_and_log("[%s:END]\n", "RECEIVED");

							}

//printf(
//	"\nHi Master, server have given this message %s\n",
//	listString);
//printStringInFormat(buf);
						}
					} // END handle data from server
				} // END connect to server
			} // END looping through file descriptors
		} // END for(;;)--and you thought it would never end!

	} //End of else if code - client process

	return 0;
}

