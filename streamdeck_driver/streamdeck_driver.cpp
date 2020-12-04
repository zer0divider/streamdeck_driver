// streamdeck_driver.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ShlObj.h>
#include "SerialCom.h"
#include <stdarg.h>

#define MAGIC_WORDS	"ccstreamdeck"	// magic words need to be received by arduino so we know that this is indeed the stream deck we are talking to
#define RECEIVE_BUFFER_SIZE 256		// buffer size for read data
#define MAX_COM_PORT 12				// number of com ports to try
#define SLEEP_TIME_FIRST_READ 500	// how long to wait until we can expect the arduino has send the magic words

// buffer to read data from COM
char READ_BUFFER[RECEIVE_BUFFER_SIZE];

#define CONFIG_FILE "streamdeck_config.txt" // configuration file for defining hot keys
#define NUM_BUTTONS 15						// number of buttons

// modifier for hotkeys
#define MODIFIER_ALT	0x01
#define MODIFIER_CTRL	0x02
#define MODIFIER_SHIFT	0x04

// windows Vk constants
#define LETTER_KEYS_START	0x41
#define NUM_KEYS_START		0x30

class Hotkey{
public:
	Hotkey(){_numInputs = 0;}
	Hotkey(char key, unsigned int modifiers = 0){set(key, modifiers);}

	void set(char key, unsigned int modifiers = 0){
		ZeroMemory(_inputs, sizeof(_inputs));
		_numInputs = 0;
		if(modifiers & MODIFIER_ALT){
			_inputs[_numInputs].type = INPUT_KEYBOARD;
			_inputs[_numInputs].ki.wVk = VK_MENU;
			_numInputs++;
		}
		if(modifiers & MODIFIER_CTRL){
			_inputs[_numInputs].type = INPUT_KEYBOARD;
			_inputs[_numInputs].ki.wVk = VK_CONTROL;
			_numInputs++;
		}
		if(modifiers & MODIFIER_SHIFT){
			_inputs[_numInputs].type = INPUT_KEYBOARD;
			_inputs[_numInputs].ki.wVk = VK_SHIFT;
			_numInputs++;
		}
		if((key >= 'a' && key <= 'z') ||
			(key >= 'A' && key <= 'Z')){
			int offset;
			if(key >= 'a' && key <= 'z')
				offset = key - 'a';
			else
				offset = key - 'A';
			_inputs[_numInputs].type = INPUT_KEYBOARD;
			_inputs[_numInputs].ki.wVk = LETTER_KEYS_START + offset;
		}
		else if((key >= '0' && key <= '9')){
			int offset = key - '0';
			_inputs[_numInputs].type = INPUT_KEYBOARD;
			_inputs[_numInputs].ki.wVk = NUM_KEYS_START + offset;
		}
		_numInputs++;

		// add keys with keyup flags
		for(int i = 0; i < _numInputs; i++){
			_inputs[_numInputs+i] = _inputs[_numInputs-i-1];
			_inputs[_numInputs+i].ki.dwFlags = KEYEVENTF_KEYUP;
		}
		_numInputs *= 2;
	}
	
	// send the hotkey keys
	void send(){
		if(_numInputs <= 0)
			return;
		UINT sent = SendInput(_numInputs, _inputs, sizeof(INPUT));
		if(sent != _numInputs){
			fprintf(stderr, "Failed to send input: 0x%x\n", HRESULT_FROM_WIN32(GetLastError()));
		}
	}
	~Hotkey(){}

private:
	INPUT _inputs[8];
	int _numInputs;
};

Hotkey KEYS[NUM_BUTTONS];

void skip_whitespace(char ** c){
	while((**c == ' ' || **c == '\t' || **c == '\n') && **c != '\0')
		(*c)++;
}

void skip_until(char ** c, char character){
	while(**c != character && **c != '\0')
		(*c)++;
}

bool is_number(char c){
	return c >= '0' && c <= '9';
}

void parse_error(int line, const char * message)
{
	fprintf(stderr, "Error in line %d: %s\n", line, message);
}

void readKeys(FILE * f){
	char line_buffer[256];
	int line = 1;
	while(fgets(line_buffer, 256, f))
	{
		char * l = line_buffer;	
		skip_whitespace(&l);
		if(*l == '#' || *l == '\0'){
			// comment or empty, skip l
		}
		else if(is_number(*l)){
			int i = atoi(l);	
			skip_until(&l, ':');
			*l++;
			skip_whitespace(&l);
			if(*l == '\0'){
				parse_error(line, "Expected ':' after button number.");
			}
			else{
				if(*l == '\0'){
					parse_error(line, "Expected Modifier (C,S,A) or key (a-z, 0-9) after ':'.");
				}else{
					char letter = '\0';
					unsigned int mod = 0;
					while(*l != '\0')
					{
						if(*l == 'A'){
							mod |= MODIFIER_ALT;
						}
						else if(*l == 'S'){
							mod |= MODIFIER_SHIFT;
						}
						else if(*l == 'C'){
							mod |= MODIFIER_CTRL;
						}
						else if (	(*l >= 'a' && *l <= 'z') ||
									(*l >= '0' && *l <= '9')){
							letter = *l;
						}else{
							parse_error(line, "Unsupported key.");
							break;
						}
						l++;
						skip_whitespace(&l);
					}
					if(letter == '\0'){
						parse_error(line, "No key given.");
					}
					else{
						KEYS[i].set(letter, mod);
						printf("button %2d: %c%c%c + %c\n", i, mod&MODIFIER_ALT ? 'A' : ' ', mod&MODIFIER_SHIFT ? 'S' : ' ', mod&MODIFIER_CTRL ? 'C' : ' ', letter);
					}
				}
			}
		}
		else{
			parse_error(line, "Expected button number.");
		}

		line++;
	}
}

int main()
{
	char path[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path);
	strcat(path, "\\" CONFIG_FILE);
	printf("Reading configuration file %s...\n", path);
	FILE * f = fopen(path, "r");
	if(!f){
		printf("Configuration file not found! Creating default %s\n", path);
		FILE * f = fopen(path, "w");
		fprintf(f,	"# Mapping button X to hotkey:\n"
					"# X: [C=CTRL][A=ALT][S=SHIFT] [a-z | 0-9]\n"
					"#\n"
					"# Example, maps button 2 to hotkey CTRL + SHIFT + B:\n"
					"# 02: CS b\n\n");
		for(int i = 0; i < NUM_BUTTONS; i++){
			char c;
			if(i > 8)
				c = 'a' + (i-9);
			else
				c = '1' + i;
			fprintf(f, "%.2d: CS %c\n", i+1, c);
			KEYS[i].set(c, MODIFIER_CTRL | MODIFIER_SHIFT);
		}
		fclose(f);
	}
	else{
		readKeys(f);
		fclose(f);
	}
	puts("");

	// try connecting on any available COM port
	Serial * sp = NULL;
	char port_buffer[32];
	int connected = 0;
	int port = 1;
	printf("Connecting...\n");
	for(; port <= MAX_COM_PORT; port++)
	{
		sprintf(port_buffer, "\\\\.\\COM%d", port);
		sp = new Serial(port_buffer);
		// connection established
		if(sp->IsConnected()){
			// wait so arduino has time to send magic words
			Sleep(SLEEP_TIME_FIRST_READ);
			
			// read magic words
			READ_BUFFER[0] = '\0';
			int bytes_read = sp->ReadData(READ_BUFFER, RECEIVE_BUFFER_SIZE-1);
			if(bytes_read < 0){
				fprintf(stderr, "Error while reading data!\n");
			}
			else if(bytes_read > 0){
				READ_BUFFER[bytes_read] = '\0';
			}
			// check if magic word is correct
			if(!strcmp(READ_BUFFER, MAGIC_WORDS)){
				// send magic word back
				if(!sp->WriteData(MAGIC_WORDS, strlen(MAGIC_WORDS)))
				{
					fprintf(stderr, "Could not send data!\n");
				}
				
				connected = 1;
				break;
			}
			// magic word not correct -> delete sp and try again with next COM port
			else{
				
				delete(sp);
				sp = NULL;
			}
		}
	}

	// finally connected?
	if(!connected){
		fprintf(stderr, "Could not connect to the arduino on any COM port!\n");
		Sleep(1000);
		exit(1);
	}
	printf("Connected on port COM %d! Waiting for input...\n", port);

	while(sp->IsConnected()){
		int bytes_read = sp->ReadData(READ_BUFFER, RECEIVE_BUFFER_SIZE-1);
		if(bytes_read < 0){
			fprintf(stderr, "Error while receiving data!\n");
		}else if(bytes_read > 0){
			for(int i = 0; i < bytes_read; i++){
				printf("Button %d\n", READ_BUFFER[i]);
				KEYS[i].send();
			}
		}
		Sleep(100);
	}
	delete(sp);
	printf("Disconnected!\n");
	printf("Exiting...");
	Sleep(1000);
    return 0;
}

