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

#define MAX_MEM_SIZE 16777216 //16MB
#define MAX_NUM_COMMANDS 100000

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

int parse_cmd_str_to_cmd_struct (char* cmd_string , COMMAND* cmd_struct ){

	get_cmd_name (cmd_string ,  cmd_struct->cmd_name ) ;
	cmd_type (cmd_struct->cmd_name , cmd_struct->cmd_type );
	cmd_struct->b_is_labled = is_labled(cmd_string,cmd_struct->label);

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
	while(fgets(cmd_line, sizeof(cmd_line), cmd_file)!=NULL) 
	{
		if (strlen(cmd_line)<=1){
			continue;
		}
		parse_cmd_str_to_cmd_struct (cmd_line,&commands_arr[i]);
		i++;
		
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

//Returns the word that is stored in 'mem[address]'.
int load_word(int address, byte* mem ){
	return  mem[address] <<24 | mem[address+1]<<16 | mem[address+2]<<8 | mem[address+3];
}

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
int execute_set_of_instructions(COMMAND* commands_arr, int* registers_arr, byte* mem){
	int instruction_counter = 1;
	int pc = 0;
	while (strcmp(commands_arr[pc].cmd_type, "H") != 0){
		execute_instruction(commands_arr[pc], commands_arr, registers_arr, mem, &pc);
		instruction_counter++;
	}
	return instruction_counter;
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
	assert(argc ==8);
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

	return 0 ;
}