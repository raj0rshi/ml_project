//This code snippet will help you to read data from arduino

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SerialPort.h"
#include <fstream>

using namespace std;

/*Portname must contain these backslashes, and remember to
replace the following com port*/
char *port_name = "\\\\.\\COM4";

//String for incoming data
char incomingData[MAX_DATA_LENGTH];

int main()
{
	SerialPort arduino(port_name);
	if (arduino.isConnected()) cout << "Connection Established" << endl;
	else cout << "ERROR, check port name";


	char buffer[4096];
	
	ofstream myFile("stream.txt", ios::app | ios::binary);
	

	int count = 0;
	int seq = 0;
	int dotcount = 0;
	int start = 1;
	while (1){
		while (arduino.isConnected()){
			//Check if data has been read or not
			int read_result = arduino.readSerialPort(incomingData, MAX_DATA_LENGTH);
			myFile.write(incomingData, read_result);

			for (int i = 0; i < read_result; i++)
				cout << incomingData[i];
			myFile.flush();
		}

	}

}