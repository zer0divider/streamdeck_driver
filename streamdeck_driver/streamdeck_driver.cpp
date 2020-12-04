// streamdeck_driver.cpp : Defines the entry point for the console application.
//

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ShlObj.h>
#include "SerialCom.h"
#include <vector>
#include <list>

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

#define DELAY_TOKEN	'$'

#define SERIAL_DELAY 100 // wait X milliseconds after each serial read
#define KEY_SEQUENCE_DELAY 1 // wait X milliseconds after each sequence update

class Hotkey{
public:
	Hotkey(){_numInputs = 0;}
	Hotkey(char key, unsigned int modifiers = 0){set(key, modifiers);}

	void set(char key, unsigned int modifiers = 0){
		_key = key;
		_mod = modifiers;
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

	// print hotkey to stdout
	void print(){
		if(_mod&MODIFIER_CTRL){
			printf("CTRL+");
		}
		if(_mod&MODIFIER_SHIFT){
			printf("SHIFT+");
		}
		if(_mod&MODIFIER_ALT){
			printf("ALT+");
		}
		printf("%c", _key);
	}

private:
	char _key;
	unsigned int _mod;
	INPUT _inputs[8];
	int _numInputs;
};

// get current system time in milliseconds
ULONGLONG getSystemMillis(){
	SYSTEMTIME t;
	FILETIME f_t;
	GetSystemTime(&t);
	SystemTimeToFileTime(&t, &f_t);
	ULARGE_INTEGER millis;
	millis.u.HighPart =  f_t.dwHighDateTime;
	millis.u.LowPart =  f_t.dwLowDateTime;
	return millis.QuadPart/10000;
}

class HotkeySequence{
public:
	HotkeySequence(){}
	~HotkeySequence(){clear();}

	// add another hotkey with given pre-delay, key is freed on sequence destruction
	void add(Hotkey * key, int delay){
		_keys.push_back(TimedKey(key, delay));
	}

	void clear(){
		for(unsigned int i = 0; i < _keys.size(); i++){
			delete _keys[i].key;
		}
		_keys.clear();
	}

	// start sequence
	void start(){
		if(_keys.size() == 0)
			return;
		_currentKeyIndex = 0; 
		_accumDelay = 0;
		_startTime = getSystemMillis();
		_running = true;
	}

	void update(){
		if(!_running)
			return;

		TimedKey & k = _keys[_currentKeyIndex];

		// calculate delta time since sequence start
		int delta = static_cast<int>(getSystemMillis() - _startTime);

		// next key?
		if(delta > _accumDelay+k.delay){
			_accumDelay += k.delay;
			if(k.key != NULL)
				k.key->send();
			
			// check if sequence complete
			_currentKeyIndex++;
			if((unsigned int)_currentKeyIndex >= _keys.size())
				_running = false;
		}
	}

	void stop(){ _running = false; }

	bool isRunning(){ return _running;}

	// print mapping to stdout
	void print(){
		for(unsigned int i = 0; i < _keys.size(); i++){
			if(i != 0 || _keys[i].delay > 0){
				printf("%c%d ", DELAY_TOKEN, _keys[i].delay);
			}
			if(_keys[i].key == NULL){
				printf("-");
			}
			else{
				_keys[i].key->print();
			}
			printf(" ");
		}
	}

private:
	struct TimedKey{
		TimedKey(Hotkey * _key, int _delay): key(_key), delay(_delay){}
		int delay;
		Hotkey * key;
	};
	std::vector<TimedKey> _keys;
	int _currentKeyIndex;
	ULONGLONG _accumDelay; // delay accumulated throughout the sequence (in millis)
	ULONGLONG _startTime; // when was the sequence started
	bool _running; // currently run

};

HotkeySequence KEYS[NUM_BUTTONS];

bool is_whitespace(char c){
	return (c == ' ' || c == '\t' || c == '\n');
}

void skip_whitespace(char ** c){
	while(is_whitespace(**c) && **c != '\0')
		(*c)++;
}

void skip_until_whitespace(char ** c){
	while(!is_whitespace(**c) && **c != '\0')
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

void add_key(int i, char letter, unsigned int mod, int delay)
{
	KEYS[i].add(new Hotkey(letter, mod), delay);
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
			int i = atoi(l)-1;
			if(i < 0 || i >= NUM_BUTTONS){
				parse_error(line, "Invalid button id.");
				continue;
			}
			KEYS[i].clear();
			skip_until(&l, ':');
			if(*l == '\0'){
				parse_error(line, "Expected ':' after button number.");
			}
			else{
				*l++;
				skip_whitespace(&l);
				if(*l == '\0'){
					parse_error(line, "Expected Modifier (C,S,A) or key (a-z, 0-9) or delay ($) after ':'.");
				}else{
					char letter = '\0';
					unsigned int mod = 0;
					int delay = 0;
					bool last_was_delay = false;
					while(*l != '\0')
					{
						if(is_whitespace(*l)){
							if(!last_was_delay){//add key
								if(letter == '\0'){
									parse_error(line, "No key given.");
									break;
								}
								else{
									add_key(i, letter, mod, delay);
								}
								letter = '\0';
								delay = 0;
								mod = 0;
							}
							skip_whitespace(&l);
						}
						else if(*l == '$'){
							l++;
							if(letter != '\0' || mod != 0){
								parse_error(line, "Expected whitespace before '$'");
								break;
							}
							if(!is_number(*l))
							{
								parse_error(line, "Expected digit after '$'.");
								break;
							}
							delay = atoi(l);
							skip_until_whitespace(&l);
							last_was_delay = true;
						}
						else{
							last_was_delay = false;
							if(*l == 'A'){
								mod |= MODIFIER_ALT;
								l++;
							}
							else if(*l == 'S'){
								mod |= MODIFIER_SHIFT;
								l++;
							}
							else if(*l == 'C'){
								mod |= MODIFIER_CTRL;
								l++;
							}
							else if (	(*l >= 'a' && *l <= 'z') ||
										(*l >= '0' && *l <= '9')){
								letter = *l;
								l++;
							}else{
								parse_error(line, "Unsupported key.");
								break;
							}
						}
					}
					// add last (if not already)
					if(letter != '\0'){
						add_key(i, letter, mod, delay);
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
			fprintf(f, "%.2d: CS%c\n", i+1, c);
			KEYS[i].add(new Hotkey(c, MODIFIER_CTRL | MODIFIER_SHIFT), 0);
		}
		fclose(f);
	}
	else{
		readKeys(f);
		fclose(f);
	}
	for(int i = 0; i < NUM_BUTTONS; i++){
		printf("Button %2d: ", i+1);
		KEYS[i].print();
		puts("");
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
	std::list<int> active_buttons;
	while(sp->IsConnected()){
		int bytes_read = sp->ReadData(READ_BUFFER, RECEIVE_BUFFER_SIZE-1);
		if(bytes_read < 0){
			fprintf(stderr, "Error while receiving data!\n");
		}else if(bytes_read > 0){
			for(int i = 0; i < bytes_read; i++){
				int button_id = READ_BUFFER[i];
				if(button_id >= NUM_BUTTONS){
					fprintf(stderr, "Invalid button id %d!\n", button_id+1);
				}
				else{
					if(KEYS[button_id].isRunning()){
						printf("Button %d still active!\n", button_id+1);
					}
					else{
						printf("Button %d\n", button_id+1);
						KEYS[button_id].start();
						active_buttons.push_back(button_id);
					}
				}
			}
		}

		if(active_buttons.size() > 0)
		{
			std::list<int>::iterator it = active_buttons.begin();
			while(it != active_buttons.end()){
				int i = *it;
				KEYS[i].update();
				if(!KEYS[i].isRunning()){
					it = active_buttons.erase(it);
				}
				else{
					it++;
				}
			}
			Sleep(KEY_SEQUENCE_DELAY);
		}
		else
		{
			Sleep(SERIAL_DELAY);
		}
	}
	delete(sp);
	printf("Disconnected!\n");
	printf("Exiting...");
	Sleep(1000);
    return 0;
}

