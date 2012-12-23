// simulator.cpp : Defines the entry point for the console application.

/*
******************************************
**	Idan Sayag 200400885				**
**	Yasmin Slonimsky 200343457			**
******************************************
*/

#include "stdafx.h"
#include <string.h> 
#include <Windows.h>
#include <assert.h>
#include <math.h>

#define MAX_MEM_SIZE 16777216 //16MB
#define MAX_NUM_COMMANDS 100000

#define L1_L2_BUS_WIDTH 4 //4 bytes = 32bits
#define L2_MEM_BUS_WIDTH 4//4 bytes = 32bits
#define WORD_SIZE 4

typedef struct {
	char cmd_name [10];
	char cmd_type [2] ; 
	BOOL b_is_labled ; 
	char label [256];
	char cmd_Reg[3][10] ;
	int Rs   ; // valid only for R_type and I_type 
	int Rt  ; // valid only for R_type and I_type
	int Rd  ; // valid only for R_type 
	char immediate [256] ; //it could be either a number(addi ... ) or a label (beq ...)  valid only for I_type
	char address_label [256] ; // valid only for J_type
	unsigned inst_address;
} COMMAND ;

typedef struct{
	//Pipeline Configurations
	int addsub_delay ;  
	int mul_delay ; 
	int div_delay ; 
	int instruction_q_depth ; 
	int addsub_rs ; 
	int muldiv_rs ; 
	int load_q_depth ; 
	int reorder_buffer ;  

	//Branch Prediction Configurations
	int ghr_width ;

	//Multi Threading Configurations
	int two_threads_enabled ;

	//Memory Configurations
	int l1_block_size ;
	int l1_access_delay ;
	int l1_cache_size  ;
	int l2_block_size  ;
	int l2_access_delay ; 
	int l2_cache_size ;
	int mem_access_delay ; 
}CONFIG ;

typedef struct {
	unsigned tag;
	byte valid;
	byte* block;
	unsigned first_block_address;
	unsigned last_block_address;
	unsigned critical_word_offset;
	int block_trans_start_time;
	int block_trans_end_time;
}L1_BLOCK;

typedef struct {
	unsigned tag[2];
	byte valid[2];
	unsigned first_block_address[2];
	unsigned last_block_address[2];
	byte LRU;
	byte dirty[2];
	byte* block[2];
	unsigned critical_word_offset[2];
	int block_trans_start_time[2];
	int block_trans_end_time[2];
}L2_BLOCK;

typedef struct{
	L1_BLOCK* block_arr;
	int hit_counter;
	int miss_counter;
	int access_counter;
}L1_CACHE;

typedef struct{
	L2_BLOCK* block_arr;
	int hit_counter;
	int miss_counter;
	int access_counter;
}L2_CACHE;


BOOL is_labled (char* cmd , char* label ) {
	int i , j ;
	BOOL b_is_labled = false ;
	for(i=0; i< strlen(cmd);i++){
		if (cmd[i]==':'){
			b_is_labled = true ;
			for (j=0;j<i;j++){
				label[j]=cmd[j];
			}
			label[j] = '\0' ;
			return b_is_labled ;
		}
	}
	return b_is_labled ;
}

int get_cmd_name (char* cmd , char* cmd_name ){
	char label [256] ; 
	int i=0, j=0 , k=0 , m=0  ;
	BOOL b_lable = is_labled( cmd , label ) ; 
	if (b_lable == false){
		for(i=0 ; i< strlen(cmd)&&(cmd[i]==32 ||cmd[i]=='\t'); i++ ); // 32 = white space \t= tab 
		for(j=i,k=0;j<strlen(cmd) && cmd[j] != 32 && cmd[j]!='\t' ; j++,k++){
			cmd_name[k] = cmd[j] ;
			}
		cmd_name[k] = '\0';
			
	}else{//cmd has label 
		for(i=0 ; i< strlen(cmd) && cmd[i]!= ':'; i++ );
		for (j= i+1 ; (cmd[j]==32 ||cmd[j]=='\t') && j < strlen(cmd); j++ ) ; 
		for(k=j,m=0;k<strlen(cmd) && cmd[k] != 32 && cmd[k]!='\t' ; k++,m++){
			cmd_name[m] = cmd[k] ;
		}
		cmd_name[m] = '\0';
	}
	
	return 1 ;
		
}

int cmd_type (char* cmd_name, char* cmd_type ){
	if (stricmp(cmd_name,"add")==0 || stricmp(cmd_name,"sub")==0 || stricmp(cmd_name,"mul")==0 || stricmp(cmd_name,"div")==0 || stricmp(cmd_name,"slt")==0 )
		strcpy(cmd_type,"R");
	else if (stricmp(cmd_name,"addi")==0 || stricmp(cmd_name,"subi")==0 || stricmp(cmd_name,"lw")==0 || stricmp(cmd_name,"sw")==0 || stricmp(cmd_name,"beq")==0 || stricmp(cmd_name,"bne")==0 || stricmp(cmd_name,"slti")==0 )
		strcpy(cmd_type,"I");
	else if (stricmp(cmd_name,"j")==0 || stricmp(cmd_name,"jr")==0 )
		strcpy(cmd_type,"J");
	else
		strcpy(cmd_type,"H");

	cmd_type[1]='\0' ;
	return 1 ;

}
int update_R_type(char* cmd_string,COMMAND* cmd_struct ){
	int i=0 , n= 0,m=0 ; 
	int counter = 0 ;
	for(i=0;i<strlen(cmd_string)&& counter< 3;i++ ){
		if(cmd_string[i]=='$'){
			for(n=i+1;n<strlen(cmd_string)&& cmd_string[n]!= 32 && cmd_string[n]!= ',' && cmd_string[n]!= '\t';n++){
				cmd_struct->cmd_Reg[counter][m] = cmd_string[n] ;
				m++ ;
			}
			m = 0 ;
			i=n;
			counter ++ ; 

		}
	}
	cmd_struct->Rd = atoi(cmd_struct->cmd_Reg[0]);
	cmd_struct->Rs = atoi(cmd_struct->cmd_Reg[1]);
	cmd_struct->Rt = atoi(cmd_struct->cmd_Reg[2]);
	
	return 1 ;
}
int update_I_type(char* cmd_string,COMMAND* cmd_struct ){
	int i = 0 , j=0,k=0,n=0,m=0 ;
	int counter = 0 ;
	if(strcmp(cmd_struct->cmd_name ,"lw")==0 || (strcmp(cmd_struct->cmd_name ,"sw")==0)){//cmd of the form :  lw $1,(100)$2
		for(i=0;i<strlen(cmd_string)&& counter< 2;i++ ){
			if(cmd_string[i]=='$'){
				for(n=i+1;n<strlen(cmd_string)&& cmd_string[n]!= 32 && cmd_string[n]!= ',' && cmd_string[n]!= '\t';n++){
					cmd_struct->cmd_Reg[counter][m] = cmd_string[n] ;
					m++ ;
				}
				m = 0 ;
				i=n;
				counter ++ ; 

			}else if (cmd_string[i]=='('){
				for(j=i+1;j<strlen(cmd_string) && cmd_string[j] != ')'  ; j++ ){
					if ( cmd_string[j] != 32 && cmd_string[j] != '\t' ){
						cmd_struct->immediate[k] = cmd_string[j];
						k++ ;
					}
					cmd_struct->immediate[k]='\0';

				}
				i = j ;
			}
		}
	}else{//cmd is addi/subi/beq/bne/slti and of form slti $1,$2, 100 
		for(i=0;i<strlen(cmd_string)&& counter< 2;i++ ){
			if(cmd_string[i]=='$'){
				for(n=i+1;n<strlen(cmd_string)&& cmd_string[n]!= 32 && cmd_string[n]!= ',' && cmd_string[n]!= '\t';n++){
					cmd_struct->cmd_Reg[counter][m] = cmd_string[n] ;
					m++ ;
				}
				m = 0 ;
				i=n;
				counter ++ ; 

			}
		}
		k=0 ;
		for (j= i ; j<strlen(cmd_string);j++){
			if (cmd_string[j]!=',' && cmd_string[j] !=32 && cmd_string[j] !='\t' && cmd_string[j] != '\n'){
				cmd_struct->immediate[k] = cmd_string[j];
				k++ ;

			}
		}
		cmd_struct->immediate[k]='\0';
	}

	cmd_struct->Rt = atoi(cmd_struct->cmd_Reg[0]);
	cmd_struct->Rs = atoi(cmd_struct->cmd_Reg[1]);

	return 1 ;
}
int update_J_type(char* cmd_string,COMMAND* cmd_struct){
	int i=0 , k=0 ,m=0 ; 
	if(strcmp(cmd_struct->cmd_name ,"j")==0){
		for(i=0; i<strlen(cmd_string)&&cmd_string[i] != 'j'  ; i++ );
			for (k= i+1 ; k <strlen(cmd_string);k++){
				if ( cmd_string[k] !=32 && cmd_string[k] !='\t' && cmd_string[k] !='\n'){
					cmd_struct->address_label[m] = cmd_string[k];
					m++;
				}		

			}
		cmd_struct->address_label[m] = '\0';
	}else if (strcmp(cmd_struct->cmd_name ,"jr")==0){
		for(i=0;i<strlen(cmd_string)&&cmd_string[i] != '$';i++);
		for(k=i+1;k<strlen(cmd_string)&& cmd_string[k]!= 32 && cmd_string[k]!= ',' && cmd_string[k]!= '\t';k++){
			cmd_struct->cmd_Reg[0][m] = cmd_string[k] ;
			m++ ;
		 
		}
		cmd_struct->Rs = atoi(cmd_struct->cmd_Reg[0]);
	}
	return 1 ;
}

int parse_cmd_str_to_cmd_struct (char* cmd_string , COMMAND* cmd_struct,unsigned address ){

	get_cmd_name (cmd_string ,  cmd_struct->cmd_name ) ;
	cmd_type (cmd_struct->cmd_name , cmd_struct->cmd_type );
	cmd_struct->b_is_labled = is_labled(cmd_string,cmd_struct->label);
	cmd_struct->inst_address = address;
	if (strcmp(cmd_struct->cmd_type,"R")==0){
		update_R_type(cmd_string,cmd_struct );
	}
	else if (strcmp(cmd_struct->cmd_type,"I")==0){
		update_I_type(cmd_string,cmd_struct );
	}
	else if (strcmp(cmd_struct->cmd_type,"J")==0){
		update_J_type(cmd_string,cmd_struct); 
	}

	//else the command is HALT and we don't have to update COMMAND struct.


	return 1 ; 

}

//Reads 'cmd_file' line by line. Each command line is parsed to 'COMMAND' struct and inserted into 'commands_arr'. 
void parse_cmd_file(FILE* cmd_file, COMMAND* commands_arr){
	char cmd_line[256];
	int i = 0;
	unsigned address = 0x00F00000;
	while(fgets(cmd_line, sizeof(cmd_line), cmd_file)!=NULL) 
	{
		if (strlen(cmd_line)<=1){
			continue;
		}
		parse_cmd_str_to_cmd_struct (cmd_line,&commands_arr[i],address);
		i++;
		address+=0x4;
	}
}

//Loads 'mem_init_file' into 'mem_array'.
void load_memory(FILE* mem_init_file ,byte* mem_array){
	char line[257];
	char* token ;
	int i =0 ;
	
	while(fgets(line,256,mem_init_file) !=NULL){
		token = strtok( line, " "  );
		while( token != NULL ) {
			(mem_array[i]) = strtol(token,NULL,16);
			i++ ;
			token = strtok( NULL, " "  );		
		}
	}

}
void parse_address (unsigned* offset, unsigned* index, unsigned* tag, unsigned address, int block_size, int cache_size){
	int offset_length = (int)(log((double)block_size)/log(2.0));
	int index_length = (int)(log((double)(cache_size/block_size))/log(2.0));
	int tag_length = 32-offset_length-index_length;
	unsigned mask = 0;
	
	//extract offset
	for (int i = 0 ; i < offset_length-1; i++){
		mask++;
		mask = mask<<1;
	}
	mask++;
	*offset = address & mask;
	mask = 0;
	//extract index
	for (int i = 0 ; i < index_length-1; i++){
		mask++;
		mask = mask<<1;
	}
	mask++;
	mask = mask<<offset_length;
	*index = (address & mask)>>offset_length;
	mask = 0;
	//extract tag
	for (int i = 0 ; i < tag_length-1; i++){
		mask++;
		mask = mask<<1;
	}
	mask++;
	mask = mask<<(index_length+offset_length);
	*tag = (address & mask)>>(index_length+offset_length);

}

bool is_in_L1(unsigned index, unsigned tag, unsigned offset, L1_CACHE* L1_cache, int* current_time, CONFIG* config){

	L1_cache->access_counter++;
	int critical_word_index = L1_cache-> block_arr[index].critical_word_offset/4;
	int requested_word_index = offset/4;
	int nWordsRead; 
	int block_size_words = config->l1_block_size/4;

	if((L1_cache->block_arr[index].tag == tag) && (L1_cache->block_arr[index].valid)){
		if(*current_time >= L1_cache->block_arr[index].block_trans_end_time){//requested block is cached and available
			*current_time += config->l1_access_delay;
			L1_cache->hit_counter++;
			return true;
		}else{//requested block is in tranfer progress
			nWordsRead = (*current_time-L1_cache->block_arr[index].block_trans_start_time)*L1_L2_BUS_WIDTH/4;
			if (requested_word_index > critical_word_index){
				if (requested_word_index < critical_word_index + nWordsRead){		
					*current_time += config->l1_access_delay;
					L1_cache->hit_counter++;
					return true;
				}else{
					*current_time += config->l1_access_delay + ((requested_word_index+1) - (critical_word_index+nWordsRead))/(L1_L2_BUS_WIDTH/4);
					L1_cache->miss_counter++;
					return true;
				}
			}else{
				if (critical_word_index+nWordsRead<=block_size_words){
					*current_time += config->l1_access_delay + ((block_size_words- (critical_word_index+nWordsRead))+requested_word_index+1)/(L1_L2_BUS_WIDTH/4);
					L1_cache->miss_counter++;
					return true;
				}else{
					*current_time += config->l1_access_delay + ((requested_word_index+1)- (critical_word_index+nWordsRead)%block_size_words)/(L1_L2_BUS_WIDTH/4);
					L1_cache->miss_counter++;
					return true;
				}
			}
		}
	}
	*current_time += config->l1_access_delay;
	L1_cache->miss_counter++;
	return false;
}


bool is_in_L2(unsigned index, unsigned tag, unsigned offset, L2_CACHE* L2_cache, int* current_time, CONFIG* config){

	L2_cache->access_counter++;
	int critical_word_index;	
	int requested_word_index = offset/4;
	int nWordsRead; 
	int block_size_words = config->l2_block_size/4;
	int way;

	if((L2_cache->block_arr[index].tag[0] == tag) && (L2_cache->block_arr[index].valid[0])){
		way = 0;
	}else if ((L2_cache->block_arr[index].tag[1] == tag) && (L2_cache->block_arr[index].valid[1])){
		way = 1;
	}else{
		*current_time += config->l2_access_delay; 
		L2_cache->miss_counter++;
		return false;
	}

	L2_cache->block_arr[index].LRU = !way;
	critical_word_index = L2_cache->block_arr[index].critical_word_offset[way]/4;

	if(*current_time >= L2_cache->block_arr[index].block_trans_end_time[way]){//requested block is cached and available
		*current_time += config->l2_access_delay;
		L2_cache->hit_counter++;
		return true;
	}else{//requested block is in tranfer progress
		nWordsRead = (*current_time-L2_cache->block_arr[index].block_trans_start_time[way])*L2_MEM_BUS_WIDTH/4;
		if (requested_word_index > critical_word_index){
			if (requested_word_index < critical_word_index + nWordsRead){		
				*current_time += config->l2_access_delay;
				L2_cache->hit_counter++;
				return true;
			}else{
				*current_time += config->l2_access_delay + ((requested_word_index+1) - (critical_word_index+nWordsRead))/(L2_MEM_BUS_WIDTH/4);
				L2_cache->miss_counter++;
				return true;
			}
		}else{
			if (critical_word_index+nWordsRead<=block_size_words){
				*current_time += config->l2_access_delay + ((block_size_words- (critical_word_index+nWordsRead))+requested_word_index+1)/(L2_MEM_BUS_WIDTH/4);
				L2_cache->miss_counter++;
				return true;
			}else{
				*current_time += config->l2_access_delay + ((requested_word_index+1)- (critical_word_index+nWordsRead)%block_size_words)/(L2_MEM_BUS_WIDTH/4);
				L2_cache->miss_counter++;
				return true;
			}
		}
	}
}


void L2_to_L1_trans(L1_CACHE* L1_cache, unsigned offset_L1, unsigned index_L1, unsigned tag_L1,L2_CACHE* L2_cache, unsigned offset_L2, unsigned index_L2, unsigned tag_L2,
					int current_time, CONFIG* config, int way){
	int i_L1;
	int i_L2;

	L1_cache->block_arr[index_L1].tag = tag_L1;
	L1_cache->block_arr[index_L1].critical_word_offset = offset_L1;
	L1_cache->block_arr[index_L1].block_trans_start_time = current_time;
	L1_cache->block_arr[index_L1].block_trans_end_time = current_time + config->l1_block_size/L1_L2_BUS_WIDTH;
	L1_cache->block_arr[index_L1].valid = 1;


	for (int i = 0 ; i < config->l1_block_size; i++){
		i_L1 = (offset_L1+i)%config->l1_block_size;
		i_L2 = (offset_L2+i)%config->l2_block_size;
		L1_cache->block_arr[index_L1].block[i_L1]=L2_cache->block_arr[index_L2].block[way][i_L2];
	}
		
}

void MEM_to_L2_trans(byte* mem, L2_CACHE* L2_cache, unsigned offset_L2, unsigned index_L2, unsigned tag_L2, 
	int current_time, CONFIG* config, unsigned address,int way,bool is_inst){
	
	int i_L2;

	L2_cache->block_arr[index_L2].tag[way] = tag_L2;
	L2_cache->block_arr[index_L2].critical_word_offset[way] = offset_L2;
	L2_cache->block_arr[index_L2].block_trans_start_time[way] = current_time;
	L2_cache->block_arr[index_L2].block_trans_end_time[way] = current_time + config->l2_block_size/L1_L2_BUS_WIDTH;
	L2_cache->block_arr[index_L2].valid[way] = 1;
	L2_cache->block_arr[index_L2].LRU = !way;

	if (!is_inst){
		for (int i = 0 ; i < config->l2_block_size; i++){
			i_L2 = (offset_L2+i)%config->l2_block_size;
			L2_cache->block_arr[index_L2].block[way][i_L2] = mem[address + i];
		}
	}else{
		L2_cache->block_arr[index_L2].block[way][(offset_L2+3)%config->l2_block_size] = address & 0xff000000;
		L2_cache->block_arr[index_L2].block[way][(offset_L2+2)%config->l2_block_size] = address & 0x00ff0000;
		L2_cache->block_arr[index_L2].block[way][(offset_L2+1)%config->l2_block_size] = address & 0x0000ff00;
		L2_cache->block_arr[index_L2].block[way][(offset_L2)%config->l2_block_size] = address & 0x000000ff;
	}
		
}

//Returns the word that is stored in 'mem[address]'.
int load_word(unsigned address, byte* mem, L1_CACHE* L1_cache, L2_CACHE* L2_cache,CONFIG* config,int* current_time){
	unsigned offset_L1;
	unsigned index_L1;
	unsigned tag_L1;
	unsigned offset_L2;
	unsigned index_L2;
	unsigned tag_L2;
	int way;
	int mem_to_L2_end_time_last_trans;
	byte* block;

	parse_address(&offset_L1,&index_L1,&tag_L1,address,config->l1_block_size,config->l1_cache_size);
	parse_address(&offset_L2,&index_L2,&tag_L2,address,config->l2_block_size,config->l2_cache_size);

		if (is_in_L1(index_L1, tag_L1,offset_L1,L1_cache, current_time, config)){
			block = L1_cache->block_arr[index_L1].block;
			return block[offset_L1]<<24 | block[offset_L1+1]<<16 | block[offset_L1+2]<<8 | block[offset_L1+3];
		}else if (is_in_L2(index_L2, tag_L2,offset_L2,L2_cache,current_time,config)){
			way = !L2_cache->block_arr[index_L2].LRU;
			block = L2_cache->block_arr[index_L1].block[way];
			L2_to_L1_trans(L1_cache, offset_L1,index_L1,tag_L1,L2_cache,offset_L2,index_L2,tag_L2,*current_time,config,way);
			return block[offset_L2]<<24 | block[offset_L2+1]<<16 | block[offset_L2+2]<<8 | block[offset_L2+3];
		}else{
			way = !L2_cache->block_arr[index_L2].LRU;
			MEM_to_L2_trans(mem, L2_cache, offset_L2, index_L2, tag_L2,*current_time,config,address,way,false);
			way = !L2_cache->block_arr[index_L2].LRU;
			mem_to_L2_end_time_last_trans = L2_cache->block_arr[index_L2].block_trans_end_time[way];
			L2_to_L1_trans(L1_cache, offset_L1,index_L1,tag_L1,L2_cache,offset_L2,index_L2,tag_L2,mem_to_L2_end_time_last_trans,config,way);
			return  mem[address] <<24 | mem[address+1]<<16 | mem[address+2]<<8 | mem[address+3];
		}
	
}
/*
//Stores 'val' in 'mem[address]'.
void store_word (int val , unsigned address,byte* mem,L1_CACHE* L1_cache, L2_CACHE* L2_cache,CONFIG* config,int* current_time){

	unsigned offset_L1;
	unsigned index_L1;
	unsigned tag_L1;
	unsigned offset_L2;
	unsigned index_L2;
	unsigned tag_L2;
	byte* block;

	parse_address(&offset_L1,&index_L1,&tag_L1,address,config->l1_block_size,config->l1_cache_size);
	parse_address(&offset_L2,&index_L2,&tag_L2,address,config->l2_block_size,config->l2_cache_size);

	if (is_in_L1(index_L1, tag_L1,offset_L1,L1_cache, current_time, config)){
		L1_cache->block_arr[index_L1].block[(offset_L1+3)%config->l1_block_size] = val & 0xff000000;
		L1_cache->block_arr[index_L1].block[(offset_L1+2)%config->l1_block_size] = val & 0x00ff0000;
		L1_cache->block_arr[index_L1].block[(offset_L1+1)%config->l1_block_size] = val & 0x0000ff00;
		L1_cache->block_arr[index_L1].block[(offset_L1)%config->l1_block_size] = val & 0x000000ff;
	}else{
	}

}*/

//Stores 'val' in 'mem[address]'.
void store_word (int val , int address,byte* mem){
	mem[address+3] = val & 0xff000000 ;
	mem[address+2] = val & 0x00ff0000 ;
	mem[address +1] = val & 0x0000ff00 ;
	mem[address] = val & 0x000000ff ;
}

//Executes 'cmd' and increases program counter 'pc'.
void execute_instruction(COMMAND cmd, COMMAND* commands_arr, int* registers_arr, byte* mem, int* pc){
	/*Instruction Type: R-type*/
	if (strcmp(cmd.cmd_type,"R") == 0){
		if(stricmp(cmd.cmd_name,"add") == 0){
			registers_arr[cmd.Rd] = registers_arr[cmd.Rs]+ registers_arr[cmd.Rt];
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"sub") == 0){
			registers_arr[cmd.Rd] = registers_arr[cmd.Rs] - registers_arr[cmd.Rt];
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"mul") == 0){
			registers_arr[cmd.Rd] = registers_arr[cmd.Rs]*registers_arr[cmd.Rt];
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"div") == 0){
			registers_arr[cmd.Rd] = registers_arr[cmd.Rs]/registers_arr[cmd.Rt];
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"slt") == 0){
			if (registers_arr[cmd.Rs] < registers_arr[cmd.Rt]){
				registers_arr[cmd.Rd] = 1;
			}else{
				registers_arr[cmd.Rd] = 0;
			}
			(*pc)++;
		}
	}else if (strcmp(cmd.cmd_type,"I") == 0){/*Instruction Type: I-type*/
		if (stricmp(cmd.cmd_name,"addi") == 0){
			registers_arr[cmd.Rt] = registers_arr[cmd.Rs] + atoi(cmd.immediate);
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"subi") == 0){
			registers_arr[cmd.Rt]= registers_arr[cmd.Rs] - atoi(cmd.immediate);
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"lw") == 0){
			int address = registers_arr[cmd.Rs] + atoi(cmd.immediate);
			registers_arr[cmd.Rt] = load_word(address, mem);
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"sw") == 0){
			int address = registers_arr[cmd.Rs] + atoi(cmd.immediate);
			store_word (registers_arr[cmd.Rt], address, mem);
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"slti") == 0){
			if (registers_arr[cmd.Rs] < atoi(cmd.immediate)){
				registers_arr[cmd.Rt] = 1;
			}else{
				registers_arr[cmd.Rt] = 0;
			}
			(*pc)++;
		}else if (stricmp(cmd.cmd_name,"beq") == 0){
			if (cmd.Rs == cmd.Rt){
				for(int i = 0; strcmp(commands_arr[i].cmd_type, "H") != 0; i++){
					if (commands_arr[i].b_is_labled){
						if (strcmp(cmd.immediate , commands_arr[i].label) == 0){
							*pc = i;
						}
					}else{
						continue;
					}
				}
			}else{
				(*pc)++;
			}
		}else if (stricmp(cmd.cmd_name,"bne") == 0){
			if (registers_arr[cmd.Rs] != registers_arr[cmd.Rt]){
				for(int i = 0; strcmp(commands_arr[i].cmd_type, "H") != 0; i++){
					if (commands_arr[i].b_is_labled){
						if (strcmp(cmd.immediate , commands_arr[i].label) == 0){
							*pc = i;
							break;
						}
					}else{
						continue;
					}
				}
			}else{
				(*pc)++;
			}
		}
	}else{/*Instruction Type: J-type*/
		if (stricmp(cmd.cmd_name,"j") == 0){
			for(int i = 0; strcmp(commands_arr[i].cmd_type, "H") != 0; i++){
					if (commands_arr[i].b_is_labled){
						if (strcmp(cmd.address_label , commands_arr[i].label) == 0){
							(*pc = i);
							break;
						}
					}else{
						continue;
					}
			}
		}else if (stricmp(cmd.cmd_name,"jr") == 0){
		//NO NEED TO IMPLEMENT FOR NOW
		}
	}
	if (registers_arr[0] !=0){// make sure that $R0=0
		registers_arr[0] = 0;
	}
}


//Executes all commmands stored in 'command_arr'.
void execute_set_of_instructions(COMMAND* commands_arr, int* registers_arr, byte* mem, int* current_time, CONFIG* config, L1_CACHE* L1_cache, L2_CACHE* L2_cache ){
	int instruction_counter = 1;
	int pc = 0;
	unsigned tag_L1;
	unsigned index_L1;
	unsigned offset_L1;
	unsigned tag_L2;
	unsigned index_L2;
	unsigned offset_L2;
	int way;
	int mem_to_L2_end_time_last_trans;
	
	while (strcmp(commands_arr[pc].cmd_type, "H") != 0){
		parse_address(&offset_L1, &index_L1, &tag_L1, commands_arr[pc].inst_address, config->l1_block_size, config->l1_cache_size);
		parse_address(&offset_L2, &index_L2, &tag_L2, commands_arr[pc].inst_address, config->l2_block_size, config->l2_cache_size);
		
		if (!is_in_L1(index_L1, tag_L1,offset_L1,L1_cache, current_time, config)){
			if (is_in_L2(index_L2, tag_L2,offset_L2,L2_cache,current_time,config)){
				L2_to_L1_trans(L1_cache, offset_L1,index_L1,tag_L1,L2_cache,offset_L2,index_L2,tag_L2,*current_time,config,way,true);
			}else{
				MEM_to_L2_trans(mem, L2_cache, offset_L2, index_L2, tag_L2,*current_time,config,NULL,way,true);
				way = !L2_cache->block_arr[index_L2].LRU;
				mem_to_L2_end_time_last_trans = L2_cache->block_arr[index_L2].block_trans_end_time[way];
				L2_to_L1_trans(L1_cache, offset_L1,index_L1,tag_L1,L2_cache,offset_L2,index_L2,tag_L2,mem_to_L2_end_time_last_trans,config,way,true);
			}
		}

		execute_instruction(commands_arr[pc], commands_arr, registers_arr, mem, &pc);
		instruction_counter++;
	}

}




void update_mem_file(FILE* mem_dump_file, byte* mem){
	char line[256];
	int num_of_lines = MAX_MEM_SIZE/8;
	for (int i = 0 ; i<num_of_lines; i++){
		sprintf(line, "%02x %02x %02x %02x %02x %02x %02x %02x\n", (mem[i*8]), (mem[i*8+1]), (mem[i*8+2]), (mem[i*8+3]),(mem[i*8+4]),(mem[i*8+5]),(mem[i*8+6]),(mem[i*8+7]));
		fprintf(mem_dump_file, line);
	}
}

void update_regs_file(FILE* regs_dump_file, int* registers_arr){
	for (int i = 0; i < 32; i++){
		fprintf(regs_dump_file, "$%d %d\n", i, registers_arr[i]);
	}
}

void update_time_file(FILE* time_file, int time){
	fprintf(time_file, "%d", time);	
}

void update_committed_file(FILE* committed_file, int committed){
	fprintf(committed_file, "%d", committed);	
}

int get_param_value (char* line ,char* param ){// addsub_delay = 1 //Number of clocks to execute an add / sub command 
	int i=0,j=0,k=0 ;
	char res[256] ;
	for (i=0; i < strlen(line) && line[i]!= '=' ; i++ ) {
		if (line[i]==32 || line[i]=='\t')
			continue ; 
		param[k] = line[i] ;
		k++ ;
	}
	param[k]= '\0' ;
	k=0 ;
	for(j=i+1 ;j<strlen(line) &&(line[j]== 32 || line[j]=='\t') ; j++ ) ;
	for(i=j ; i<strlen(line) && line[i] !=32 && line[i] !='\t'&& line[i]!='/' ; i++,k++ ){
		res[k] = line[i] ;
	}
	return (atoi(res));
}

BOOL contain_ch (char* str, char ch ){
	int i = 0 ;
	for(i=0 ; i<strlen(str);i++){
		if(str[i]==ch)
			return true ;
	}
	return false ;
}


int update_field (CONFIG* config , char* param , int val ){

	if (strcmp(param,"addsub_delay")==0){
		config->addsub_delay = val ;
		return 1 ; 
	}
	if (strcmp(param,"mul_delay")==0){
		config->mul_delay = val ;
		return 1 ; 
	}
	if (strcmp(param,"div_delay")==0){
		config->div_delay = val ;
		return 1 ; 
	}
	if (strcmp(param,"instruction_q_depth")==0){
		config->instruction_q_depth = val ;
		return 1 ; 
	}
	if (strcmp(param,"addsub_rs")==0){
		config->addsub_rs = val ;
		return 1 ; 
	}
	if (strcmp(param,"muldiv_rs")==0){
		config->muldiv_rs  = val ;
		return 1 ; 
	}
	if (strcmp(param,"load_q_depth")==0){
		config->load_q_depth= val ;
		return 1 ; 
	}
	if (strcmp(param,"reorder_buffer")==0){
		config->reorder_buffer = val ;
		return 1 ; 
	}

	if (strcmp(param,"ghr_width")==0){
		config->ghr_width = val ;
		return 1 ; 
	}
	if (strcmp(param,"two_threads_enabled")==0){
		config->two_threads_enabled = val ;
		return 1 ; 
	}
	if (strcmp(param,"l1_block_size")==0){
		config->l1_block_size = val ;
		return 1 ; 
	}
	if (strcmp(param,"l1_access_delay")==0){
		config->l1_access_delay = val ;
		return 1 ; 
	}
	if (strcmp(param,"l1_cache_size")==0){
		config->l1_cache_size = val ;
		return 1 ; 
	}
	if (strcmp(param,"l2_block_size")==0){
		config->l2_block_size = val ;
		return 1 ; 
	}
	if (strcmp(param,"l2_access_delay")==0){
		config->l2_access_delay = val ;
		return 1 ; 
	}
	if (strcmp(param,"l2_cache_size")==0){
		config->l2_cache_size = val ;
		return 1 ; 
	}
	if (strcmp(param,"mem_access_delay")==0){
		config->mem_access_delay = val ;
		return 1 ; 
	}
	
	return 0 ;
}

int load_configuration_file (FILE* config_file,CONFIG* config_struct ){
	char line[257] ; 
	int cur_val ;
	char cur_param [256] ; 
	int x = 0 ;
	while(fgets(line,256,config_file) !=NULL){
		if (strlen(line) <= 1 || contain_ch (line,'=')==false ) 
			continue ; 
	cur_val = get_param_value(line,cur_param);
	x= update_field(config_struct,cur_param,cur_val) ;
	assert(x!=0);

	}

	return 1 ;
}




int main(int argc, char* argv[])
{
	int cache_size_L1 = 128;
	int block_size_L1 = 16;
	int num_of_blocks_L1 = cache_size_L1/block_size_L1;
	L1_BLOCK block;

	int cache_size_L2 = 128;
	int block_size_L2=16;
	int num_of_blocks_L2 = cache_size_L2/block_size_L2;

	block.block_trans_start_time = 12;
	block.block_trans_end_time = 16;
	block.critical_word_offset = 8;
	block.tag = 5;
	block.valid = 1;
	
	L1_CACHE INST_CACHE_L1;
	INST_CACHE_L1.block_arr = (L1_BLOCK*)malloc(128*sizeof(L1_BLOCK));
	INST_CACHE_L1.block_arr[0] = block;

	L2_CACHE INST_CACHE_L2;
	INST_CACHE_L2.block_arr= (L2_BLOCK*)malloc(1024*sizeof(L1_BLOCK));
	
	L2_BLOCK block2;
	block2.block_trans_start_time[1]=10;
	block2.block_trans_end_time[1]= 18;
	block2.critical_word_offset[1] = 12;
	block2.LRU = 0;
	block2.tag[1]=5;
	block2.valid[1]=1;
	block2.valid[0]=0;
	
	INST_CACHE_L2.block_arr[0]=block2;

	int current_time = 11;
	CONFIG config;

	FILE* config_file = fopen("C:\\Users\\dell\\Desktop\\config_file.txt","rb");
	if (config_file == NULL){
		printf("Error openning file %s\n", config_file);
		return -1;
	}
	load_configuration_file (config_file,&config);
	is_in_L2(0, 5, 4, &INST_CACHE_L2, &current_time, &config);

	/*assert(argc ==8);
	char* cmd = argv[1];//input file name 
	char* config = argv[2];//input file name 
	char* mem_init = argv[3];//input file name 
	char* regs_dump = argv[4];//output file name 
	char* mem_dump = argv[5] ;//output file name 
	char* time = argv[6] ; //output file name 
	char* committed  = argv[7] ; //output file name

	FILE* cmd_file ;	
	FILE* config_file ; 
	FILE* mem_init_file ;
	FILE* regs_dump_file;
	FILE* mem_dump_file ; 
	FILE* time_file; 
	FILE* committed_file ; 

	COMMAND* commands_arr = (COMMAND*)malloc(sizeof(COMMAND)*MAX_NUM_COMMANDS);
	byte* mem = (byte*)calloc(MAX_MEM_SIZE,sizeof(byte));
	int* registers_arr = (int*)calloc(32,sizeof(int));
	int line_counter = 0;
	int counter = 0;
	CONFIG config_struct;

	cmd_file = fopen(cmd,"r"); 
	if (cmd_file == NULL){
		printf("Error openning file %s\n", cmd_file);
		return -1;
	}

	config_file = fopen(config,"rb");
	if (config_file == NULL){
		printf("Error openning file %s\n", config_file);
		return -1;
	}
	
	mem_init_file = fopen(mem_init,"rb") ;
	if (mem_init_file == NULL){
		printf("Error openning file %s\n", mem_init);
		return -1;
	}

	regs_dump_file = fopen(regs_dump,"w"); 
	if (regs_dump_file == NULL){
		printf("Error openning file %s\n", regs_dump);
		return -1;
	}

	mem_dump_file = fopen(mem_dump,"w");
	if (mem_dump_file == NULL){
		printf("Error openning file %s\n", mem_dump);
		return -1;
	}
	time_file = fopen(time,"w");
	if (time_file == NULL){
		printf("Error openning file %s\n", time);
		return -1;
	}
	committed_file = fopen(committed,"w");
	if (committed_file == NULL){
		printf("Error openning file %s\n", committed);
		return -1;
	}
	
	load_memory(mem_init_file ,mem);
	load_configuration_file (config_file,&config_struct);
	parse_cmd_file(cmd_file,commands_arr);
	counter = execute_set_of_instructions(commands_arr, registers_arr, mem);
	update_mem_file(mem_dump_file, mem);
	update_regs_file(regs_dump_file, registers_arr);
	update_time_file(time_file, counter);
	update_time_file(committed_file, counter);

	free(commands_arr);
	free(mem);
	free(registers_arr);

	fclose(config_file);
	fclose(mem_init_file);
	fclose(regs_dump_file);
	fclose(mem_dump_file);
	fclose(time_file);
	fclose(committed_file);
	*/
	int x;
	scanf("%d", &x);
	return 0 ;
}