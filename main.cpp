/**
 * Ruojun Hong
 * 02/21/2017
 */
#include <iostream>
#include <regex>
#include <map>
#include <iomanip>

using namespace std;

/*define constants*/
const int MAX_SYMBOL_LEN = 16;
const int MAX_COUNT = 16;
const int MAX_MEMORY_SIZE = 512;

/**
 * global variables
 */
int linenum=0;
int modulenum=0;
int modulecount=0;
int lineoffset=0;
int charoffset=0;
int temp_num_instructions=0;



bool flag = false;//flag to handle the special case when EOF is space or new line;
bool is_second_pass = false;

char* filepath;
FILE * input;
regex decimal("[0-9]+");
regex symbolname("([A-Z]|[a-z])([A-Z]|[a-z]|[0-9])*");

map<string,int> symboltable;
map<string,int> symbolmodule;
vector<string> multiple_defined_symbols;
map<string,int> symbolusecount;
vector<string> current_use_list;
map<string,int> current_def_list;
map<string,bool> is_symbol_used_tmp;

/**
 * reset global variables to initial state, only if accept multiple command line arguments as paths
 */
void reset(){
    linenum=0;
    modulenum=0;
    modulecount=0;
    temp_num_instructions=0;
    lineoffset=0;
    charoffset=0;
    flag = false;
    is_second_pass=false;

    symboltable.clear();
    symbolmodule.clear();
    multiple_defined_symbols.clear();
    symbolusecount.clear();
    current_use_list.clear();
}

/**
 * print symbol table
 */
void print_symbol_table(){
    cout<<"Symbol Table"<<endl;
    for (map<string,int>::iterator it=symboltable.begin(); it!=symboltable.end(); ++it) {
        cout << it->first << "=" << it->second;
        if(find(multiple_defined_symbols.begin(), multiple_defined_symbols.end(),it->first) != multiple_defined_symbols.end()){
            cout<<" Error: This variable is multiple times defined; first value used";
        }
        cout<<endl;
    }
}
/**
 * The function to print the errors occurred at parsing to console
 * @param errcode
 */
void __parseerror(int errcode) {
    const string errstr[] = {
            "NUM_EXPECTED", // Number expect
            "SYM_EXPECTED", // Symbol Expected
            "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
            "SYM_TOO_LONG", // Symbol Name is too long
            "TO_MANY_DEF_IN_MODULE", // > 16
            "TO_MANY_USE_IN_MODULE", // > 16
            "TO_MANY_INSTR", //total num_instr exceeds memory size (512)
    };
    cout<<"Parse Error line "<<linenum<< " offset "<<charoffset<<": "<<errstr[errcode]<<endl;
    exit(1);
}

/**
 * the tokenizer
 * @return a token as a string without spaces
 */
string tokenizer(){
    string s ="";
    char current_char;
    current_char=fgetc(input);

    if(feof(input)){
        return s;
    }
    //get rid of extra spaces &
    while(current_char==' '||current_char=='\t'||current_char=='\n'){
        if(current_char == '\n') {
            linenum++;
        };
        current_char = fgetc(input);
    }
    /*read the token until hit a space or new line*/
    while(current_char!=' '&&current_char!='\t'&&current_char!='\n'&&!feof(input)){
        s = s+current_char;
        current_char=fgetc(input);
        if(current_char == '\n') {
            linenum++;
        }
    }
    //cout<<"the token is "<<s<<endl;
    return s;
}

/**
 * read the integers from input file
 * @return an integer
 */
int read_int(){
    //cout<<"...read int"<<endl;
    int val = 0;
    string s = tokenizer();

    if(s.empty()){
        if(!flag) {
            return -1;
        }
        else{
            __parseerror(0);
        }
    }
    if(!regex_match(s,decimal)){
        __parseerror(0);//the token does not match the regex and is not an integer
    }
    else{
        val = stoi(s);
    }
    return val;
}

/**
 * read a symbol from file
 * @return the symbol as a string
 */
string read_sym(){
    //cout<<"...read symbol"<<endl;
    string s = tokenizer();


    if(s.length()>MAX_SYMBOL_LEN){
        __parseerror(3);//the token is bigger than 16 digits
    }
    if(!regex_match(s,symbolname)){
        __parseerror(1);//the token is not a legal symbol name
    }

    return s;
}

/**
 * read a definition
 */
void read_def(){
    //cout<<"...read def"<<endl;
    string str= read_sym();
    int val=read_int();
    if(!is_second_pass){
        if(symboltable.find(str)==symboltable.end())//not found
            {
                symboltable[str]=lineoffset+val;
                current_def_list[str]=lineoffset+val;
                symbolmodule[str]=modulecount;
                symbolusecount[str]=0;
            }
        else{//found
            multiple_defined_symbols.push_back(str);
        }
    }
}

/**
 * read the definition list
 */
void read_def_list(){
    int defcount=read_int();
    if (defcount==-1){
        return;
    }
    if(defcount>MAX_COUNT){
        linenum++;
        __parseerror(4);
    }
    flag = true;//Set flag to true since new module is started
    for (int i=0;i<defcount; i++) {
        read_def();
    }
}

/**
 * read a symbol on the use list
 * @return a symbol as a string
 */
string read_use(){
    string s = read_sym();
    return s;
}

/**
 * read the use list
 */
void read_use_list(){
    int usecount = read_int();

    if(usecount>MAX_COUNT){
        linenum++;
        __parseerror(5);
    }
    current_use_list.push_back(to_string(usecount));
    for(int i = 0;i<usecount;i++){
        string s = read_use();
        current_use_list.push_back(s);
    }
    /**
            * populate is_symbol_used_tmp
            */
    for (int i = 1;i<=stoi(current_use_list[0]);i++) {
        is_symbol_used_tmp[current_use_list[i]]=false;
    }
}

/**
 * read an instruction
 */
void read_inst(){
    string type =tokenizer();

    if(type.compare("A")!=0&&type.compare("E")!=0&&type.compare("I")!=0&&type.compare("R")!=0){
        __parseerror(2);
    }

    int instr = read_int();
    bool is_defined = true;
    bool is_legal_length = true;
    bool is_legal_absaddr = true;
    bool is_legal_relaaddr = true;
    string str;

    if(is_second_pass) {
        if(instr>9999){
            instr=9999;
            cout << setfill('0') << setw(3) << modulenum << ": " << setfill('0') << setw(4) << instr;
            if(type.compare("I")==0){
                cout << " Error: Illegal immediate value; treated as 9999";
            }
            else{
                cout<<" Error: Illegal opcode; treated as 9999";
            }
        }
        else {

            if (type.compare("R") == 0) {
                int tmp=instr%1000;
                if(tmp>temp_num_instructions){
                    is_legal_relaaddr=false;
                    instr = instr/1000*1000+lineoffset;
                }
                else {
                    instr = instr+lineoffset;
                }
            }
            if (type.compare("E") == 0) {
                int oprand = instr % 1000;
                if(oprand>=stoi(current_use_list[0])){
                    is_legal_length=false;
                }
                else {
                    string s = current_use_list[oprand+1];
                    is_symbol_used_tmp[s]=true;
                    if (symboltable.find(s) == symboltable.end())//not found
                    {
                        is_defined = false;
                        str = s;
                    } else {//found
                        symbolusecount[s] += 1;
                        instr =instr/1000 * 1000+ symboltable[s];
                    }
                }
            }
            if(type.compare("A")==0){
                int oprand = instr%1000;
                if (oprand>MAX_MEMORY_SIZE){
                    instr = instr-oprand;
                    is_legal_absaddr=false;
                }

            }
            cout << setfill('0') << setw(3) << modulenum << ": " << setfill('0') << setw(4) << instr;
            if (!is_defined) {
                cout << " Error: " << str << " is not defined; zero used";
            }
            if(!is_legal_length){
                cout << " Error: External address exceeds length of uselist; treated as immediate";
            }
            if(!is_legal_absaddr){
                cout<< " Error: Absolute address exceeds machine size; zero used";
            }
            if(!is_legal_relaaddr){
                cout<<" Error: Relative address exceeds module size; zero used";
            }
        }

        cout << endl;
        ++modulenum;

        current_use_list.clear();
    }
}

/**
 * read the instruction list
 */
void read_inst_list(){
    //cout<<"...read inst list"<<endl;
    int codecount = read_int();
    temp_num_instructions=codecount;
    if(lineoffset+codecount>MAX_MEMORY_SIZE){
        linenum++;
        __parseerror(6);
    }

    for(int i=0;i<codecount;i++){
        read_inst();
    }
    if(is_second_pass){
        for (auto const &ent1 : is_symbol_used_tmp) {
            if (!ent1.second) {
                cout << "Warning: Module " << modulecount << ": " << ent1.first
                     << " appeared in the uselist but was not actually used"<<endl;
            }
        }
        is_symbol_used_tmp.clear();
    }

    lineoffset+=temp_num_instructions;
}

/**
 * create the module
 */
void create_module(){
    //cout<<"...create module"<<endl;
    modulecount++;
    read_def_list();

    if(flag) {
        read_use_list();
        read_inst_list();
        for(auto const &en1:current_def_list){
            if(en1.second>lineoffset){
                symboltable[en1.first]=lineoffset-temp_num_instructions;//reset to base
                cout<<"Warning: Module "<<modulecount<<": "<<en1.first<<" too big "<<en1.second<<" (max="<<lineoffset-1<<") assume zero relative"<<endl;
            }
        }
        current_def_list.clear();
    }
    else return;
}

/**
 * first pass of the linker
 */
void first_pass() {
    input = fopen(filepath,"r");
    if (input == NULL) perror ("Error opening file");
    else {
        while (!feof(input)) {
            //get rid of extra spaces
            create_module();
            flag = false;//set flag to false to indicate that a new module is not started yet
        }
    }
    fclose(input);
}

/**
 * second pass of the linker
 */
void second_pass(){
    input = fopen(filepath,"r");
    is_second_pass = true;
    lineoffset = 0;
    modulecount=0;
    cout<<"\nMemory Map"<<endl;
    if (input == NULL) perror ("Error opening file");
    else{
        while (!feof(input)){
        create_module();
        flag = false;//set flag to false to indicate that a new module is not started yet
        }
    }
    cout<<endl;
    for (auto const &ent1 : symbolusecount) {
        if(ent1.second==0){
            cout<<"Warning: Module "<< symbolmodule[ent1.first] <<": " << ent1.first<<" was defined but never used"<<endl;
        }
    }

    fclose(input);
}

/**
 * main program
 * @param argc the count of incoming file paths
 * @param argv the input file path(s)
 * @return
 */
int main(int argc, char* argv[]) {

    for (int i = 1; i < argc; i++) {
            filepath = argv[i];
            first_pass();
            print_symbol_table();
            second_pass();
            reset();
    }

    return 0;

}