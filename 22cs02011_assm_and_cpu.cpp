#include <bits/stdc++.h>
using namespace std;
#define MAX_TOKENS 100
#define underline "\033[4m"
#define closeunderline "\033[0m"

class Assembly{
    public:
    /*converts all assembly instructions to upper case*/
    void toUpperCase(string &assembly_input){
        int assembly_input_size = assembly_input.size();
        for(int i=0;i<assembly_input_size;i++)
            if(assembly_input[i]>=97 && assembly_input[i]<=122)
                assembly_input[i] = assembly_input[i]-32;
        return;
    }
    /*splits the assembly instructions into corresponding string tokens
    example ADD R1,R2,R3; will be split into <"ADD","R1","R2","R3">*/
    void tokenizer(string &assembly_input,vector<string> &assembly_tokens){
        int assembly_input_size = assembly_input.size();
        string word = "";
        for(int i=0;i<assembly_input_size;i++){
            if(assembly_input[i] == ' ' || assembly_input[i] == ',' || assembly_input[i]==';'){
                assembly_tokens.push_back(word);
                word.erase();
            }
            else
                word += assembly_input[i];        
        }
        return;
    }
    /*splits the single line assembly instruction into lines and stores them in vector<string>*/
    void linebreaker(string &assembler_input,vector<string> &line_tokens){
        int assembly_input_size = assembler_input.size();
        string word = "";
        for(int i=0;i<assembly_input_size;i++){
            if(assembler_input[i]==';'){
                word += ';';
                line_tokens.push_back(word);
                word.erase();
            }
            else if(assembler_input[i]!=' ' || assembler_input[i]!=',')
                word += assembler_input[i];
        }
        return;
    } 
    /*maps the assembly instruction to the corresponding instruction type*/
    void instruction_mapper(map<string,char> &instruction_map){
        instruction_map["ADD"] = 'r';
        instruction_map["SUB"] = 'r';
        instruction_map["SLT"] = 'r';
        instruction_map["SLTU"] = 'r';
        instruction_map["AND"] = 'r';
        instruction_map["OR"] = 'r';
        instruction_map["XOR"] = 'r';
        instruction_map["SLL"] = 'r';
        instruction_map["SRL"] = 'r';
        instruction_map["SRA"] = 'r';

        instruction_map["ADDI"] = 'i';
        instruction_map["SLTI"] = 'i';
        instruction_map["SLTIU"] = 'i';
        instruction_map["ANDI"] = 'i';
        instruction_map["ORI"] = 'i';
        instruction_map["XORI"] = 'i';
        instruction_map["SLLI"] = 'i';
        instruction_map["SRLI"] = 'i';
        instruction_map["SRAI"] = 'i';
        instruction_map["LD"] = 'i';
        instruction_map["LW"] = 'i';
        instruction_map["LH"] = 'i';
        instruction_map["LB"] = 'i';
        instruction_map["LWU"] = 'i';
        instruction_map["LHU"] = 'i';
        instruction_map["LBU"] = 'i';
        instruction_map["JALR"] = 'i';

        instruction_map["LUI"] = 'u';
        instruction_map["AUIPC"] = 'u';

        instruction_map["SD"] = 's';
        instruction_map["SW"] = 's';
        instruction_map["SH"] = 's';
        instruction_map["SB"] = 's';

        instruction_map["BEQ"] = 'b';
        instruction_map["BNE"] = 'b';
        instruction_map["BGE"] = 'b';
        instruction_map["BGEU"] = 'b';
        instruction_map["BLT"] = 'b';
        instruction_map["BLTU"] = 'b';

        instruction_map["JAL"] = 'j';
    }
    /*maps the labels in assembly instructions to the line numbers*/
    void label_mapper(vector<string> &line_tokens,map<string,int> &label_map){
        int line_number = 0;
        for(auto line:line_tokens){
            vector<string> word_tokens;
            tokenizer(line,word_tokens);
            if(word_tokens.size()==1){
                word_tokens[0] = word_tokens[0].substr(0,word_tokens[0].size()-1);
                label_map[word_tokens[0]] = line_number;
            }
            line_number++;
        }
        return;
    }    
    /*converts register to its equivalent binary*/
    string register_binary_converter(string &reg){
        reg = reg.substr(1);
        int register_number = stoi(reg);
        if(register_number<0 || register_number>31)
            return "zzzzz";
        string s = bitset<5> (register_number).to_string();
        const auto loc1 = s.find('1');
        if(loc1!=string::npos)
            return "00000"+s.substr(loc1);
        return "00000";
    }
    /*converts hexadecimal immediate into binary*/
    string immediate_binary_converter(string &immediate){
        size_t i = (immediate[1]=='X')?2:0;
        string binary = "";
        while(immediate[i]){
            switch(immediate[i]){
                case '0':
                    binary += "0000";
                break;
                case '1':
                    binary += "0001";
                break;
                case '2':
                    binary += "0010";
                break;
                case '3':
                    binary += "0011";
                break;
                case '4':
                    binary += "0100";
                break;
                case '5':
                    binary += "0101";
                break;
                case '6':
                    binary += "0110";
                break;
                case '7':
                    binary += "0111";
                break;
                case '8':
                    binary += "1000";
                break;
                case '9':
                    binary += "1001";
                break;
                case 'A':
                    binary += "1010";
                break;
                case 'B':
                    binary += "1011";
                break;
                case 'C':
                    binary += "1100";
                break;
                case 'D':
                    binary += "1101";
                break;
                case 'E':
                    binary += "1110";
                break;
                case 'F':
                    binary += "1111";
                break;
                default:
                    cout<<"\nInvalid hexadecimal digit.\n";
                    exit(1);
            }
            i++;
        }
        return binary;
    }  
    /*converts integer immediate to hexadecimal*/
    string integer_hexadecimal_converter(int line_number){
        string binary = "";
        while(line_number!=0){
            int remainder = line_number%16;
            char ch;
            if(remainder<10)
                ch = remainder+48;
            else
                ch = remainder+55;
            binary += ch;
            line_number = line_number/16;
        }
        return "000000000000"+binary;
    }
    /*converts the instruction into binary for R type instruction*/
    string binary_converter_r_type(vector<string> &r_type){
        string opcode = "0110011";
        string function3 = "";
        string function7 = "";
        string binary32 = "";
        if(r_type[0].compare("ADD")==0){
            function3 = "000";
            function7 = "0000000";
        }
        else if(r_type[0].compare("SUB")==0){
            function3 = "000";
            function7 = "0100000";
        }
        else if(r_type[0].compare("XOR")==0){
            function3 = "100";
            function7 = "0000000";
        }
        else if(r_type[0].compare("OR")==0){
            function3 = "110";
            function7 = "0000000";
        }
        else if(r_type[0].compare("AND")==0){
            function3 = "111";
            function7 = "0000000";
        }
        else if(r_type[0].compare("SLL")==0){
            function3 = "001";
            function7 = "0000000";
        }
        else if(r_type[0].compare("SRL")==0){
            function3 = "101";
            function7 = "0000000";
        }
        else if(r_type[0].compare("SRA")==0){
            function3 = "101";
            function7 = "0100000";
        }
        else if(r_type[0].compare("SLT")==0){
            function3 = "010";
            function7 = "0000000";
        }
        else if(r_type[0].compare("SLTU")==0){
            function3 = "011";
            function7 = "0000000";
        }
        else{
            cout<<"The instruction is not R type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        string rd,rs1,rs2;
        rd = r_type[1];
        rs1 = r_type[2];
        rs2 = r_type[3];
        rd = register_binary_converter(rd);
        rs1 = register_binary_converter(rs1);
        rs2 = register_binary_converter(rs2);
        rd = rd.substr(rd.size()-5);
        rs1 = rs1.substr(rs1.size()-5);
        rs2 = rs2.substr(rs2.size()-5);
        if(rd.compare("zzzzz")==0 || rs1.compare("zzzzz")==0 || rs2.compare("zzzzz")==0){
            cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
            return "";
        }
        binary32 = function7+rs2+rs1+function3+rd+opcode;
        return binary32;
    }
    /*converts the instruction into binary for I type instruction*/
    string binary_converter_i_type(vector<string> &i_type,map<string,int> &label_map){
        int immediate_type = 0;
        string function3 = "";
        string opcode = "";
        string binary32 = "";
        string rd = "";
        string rs1 = "";
        string immediate5 = "";
        string immediate7 = "0000000";
        string immediate12 = "";
        if(i_type[0].compare("ADDI")==0){
            opcode = "0010011";
            function3 = "000";
            immediate_type = 1;
        }
        else if(i_type[0].compare("SLTI")==0){
            opcode = "0010011";
            function3 = "010";
            immediate_type = 1;
        }
        else if(i_type[0].compare("SLTIU")==0){
            opcode = "0010011";
            function3 = "011";
            immediate_type = 1;
        }
        else if(i_type[0].compare("ANDI")==0){
            opcode = "0010011";
            function3 = "111";
            immediate_type = 1;
        }
        else if(i_type[0].compare("ORI")==0){
            opcode = "0010011";
            function3 = "110";
            immediate_type = 1;
        }
        else if(i_type[0].compare("XORI")==0){
            opcode = "0010011";
            function3 = "100";
            immediate_type = 1;
        }
        else if(i_type[0].compare("SLLI")==0){
            opcode = "0010011";
            function3 = "001";
            immediate_type = 2;
            immediate7 = "0000000";
        }
        else if(i_type[0].compare("SRLI")==0){
            opcode = "0010011";
            function3 = "101";
            immediate_type = 2;
            immediate7 = "0000000";
        }
        else if(i_type[0].compare("SRAI")==0){
            opcode = "0010011";
            function3 = "101";
            immediate_type = 2;
            immediate7 = "0100000";
        }
        else if(i_type[0].compare("LB")==0){
            opcode = "0000011";
            function3 = "000";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LH")==0){
            opcode = "0000011";
            function3 = "001";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LW")==0){
            opcode = "0000011";
            function3 = "010";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LD")==0){
            opcode = "0000011";
            function3 = "011";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LBU")==0){
            opcode = "0000011";
            function3 = "100";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LHU")==0){
            opcode = "0000011";
            function3 = "101";
            immediate_type = 3;
        }
        else if(i_type[0].compare("LWU")==0){
            opcode = "0000011";
            function3 = "110";
            immediate_type = 3;
        }
        else if(i_type[0].compare("JALR")==0){
            opcode = "1100111";
            function3 = "000";
            immediate_type = 4;
        }
        else{
            cout<<"The instruction is not I type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        rd = i_type[1];
        rs1 = i_type[2];
        switch(immediate_type){
            case 1:
                immediate12 = i_type[3];
                rd = register_binary_converter(rd);
                rs1 = register_binary_converter(rs1);
                rd = rd.substr(rd.size()-5);
                rs1 = rs1.substr(rs1.size()-5);
                if(rd.compare("zzzzz")==0 || rs1.compare("zzzzz")==0){
                    cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
                    return "";
                }
                immediate12 = immediate_binary_converter(immediate12);
                immediate12 = immediate12.substr(immediate12.size()-12);
                binary32 = immediate12+rs1+function3+rd+opcode;
                return binary32;
            break;
            case 2:
                immediate5 = i_type[3];
                rd = register_binary_converter(rd);
                rs1 = register_binary_converter(rs1);
                rd = rd.substr(rd.size()-5);
                rs1 = rs1.substr(rs1.size()-5);
                if(rd.compare("zzzzz")==0 || rs1.compare("zzzzz")==0){
                    cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
                    return "";
                }
                immediate5 = immediate_binary_converter(immediate5);
                immediate5 = immediate5.substr(immediate5.size()-5);
                binary32 = immediate7+immediate5+rs1+function3+rd+opcode;
                return binary32;
            break;
            case 3:
                immediate12 = i_type[2];
                rs1 = immediate12.substr(immediate12.find('[')+1);
                rs1 = rs1.substr(0,rs1.size()-1);
                immediate12 = immediate12.substr(0,immediate12.find('['));
                cout<<rs1<<endl;
                immediate12 = immediate_binary_converter(immediate12);
                immediate12 = immediate12.substr(immediate12.size()-12);
                rd = register_binary_converter(rd);
                rs1 = register_binary_converter(rs1);
                rd = rd.substr(rd.size()-5);
                rs1 = rs1.substr(rs1.size()-5);
                if(rd.compare("zzzzz")==0 || rs1.compare("zzzzz")==0){
                    cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
                    return "";
                }
                binary32 = immediate12+rs1+function3+rd+opcode;
                return binary32;
            break;
            case 4:
                immediate12 = i_type[3];
                if(label_map.find(immediate12)!=label_map.end()){
                    int line_number = label_map[immediate12];
                    immediate12 = integer_hexadecimal_converter(line_number);
                    immediate12 = immediate12.substr(immediate12.size()-8);
                    immediate12 = "0X"+immediate12;
                    immediate12 = immediate_binary_converter(immediate12);
                    immediate12 = immediate12.substr(immediate12.size()-12);
                    rd = i_type[1];
                    rs1 = i_type[2];
                    rd = register_binary_converter(rd);
                    rs1 = register_binary_converter(rs1);
                    rd = rd.substr(rd.size()-5);
                    rs1 = rs1.substr(rs1.size()-5);
                    if(rd.compare("zzzzz")==0 || rs1.compare("zzzzz")==0){
                        cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
                        return "";
                    }
                    binary32 = immediate12+rs1+function3+rd+opcode;
                }
                else{
                    cout<<"\nThe label "<<immediate12<<" is not found.\n";
                    return "";
                }
            break;
            default:
                cout<<"The instruction is not I type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
        }
        return binary32;
    }
    /*converts the instruction into binary for B type instruction*/
    string binary_converter_b_type(vector<string> &b_type,map<string,int> &label_map){
        string opcode = "1100011";
        string function3 = "";
        string rs1 = "";
        string rs2 = "";
        string immediate5 = "";
        string immediate7 = "";
        string immediate12 = "";
        string binary32 = "";
        if(b_type[0].compare("BEQ")==0){
            function3 = "000";
        }
        else if(b_type[0].compare("BNE")==0){
            function3 = "001";
        }
        else if(b_type[0].compare("BLT")==0){
            function3 = "100";
        }
        else if(b_type[0].compare("BGE")==0){
            function3 = "101";
        }
        else if(b_type[0].compare("BLTU")==0){
            function3 = "110";
        }
        else if(b_type[0].compare("BGEU")==0){
            function3 = "111";
        }
        else{
            cout<<"\nThe instruction is not B type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        rs1 = b_type[1];
        rs2 = b_type[2];
        rs1 = register_binary_converter(rs1);
        rs2 = register_binary_converter(rs2);
        rs1 = rs1.substr(rs1.size()-5);
        rs2 = rs2.substr(rs2.size()-5);
        if(rs1.compare("zzzzz")==0 || rs2.compare("zzzzz")==0){
            cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
            return "";
        }
        immediate12 = b_type[3];
        if(label_map.find(immediate12)!=label_map.end()){
            int line_number = label_map[immediate12];
            immediate12 = integer_hexadecimal_converter(line_number);
            immediate12 = immediate12.substr(immediate12.size()-8);
            immediate12 = "0X"+immediate12;
            immediate12 = immediate_binary_converter(immediate12);
            immediate12 = immediate12.substr(immediate12.size()-12);
            immediate7 += immediate12[0]+immediate12.substr(2,6);
            immediate5 += immediate12[1]+immediate12.substr(8,4);
        }
        else{
            cout<<"\nThe label "<<immediate12<<" is not found.\n";
            return "";
        }
        binary32 = immediate7+rs2+rs1+function3+immediate5+opcode;
        return binary32;
    }
    /*converts the instruction into binary for J type instruction*/
    string binary_converter_j_type(vector<string> &j_type,map<string,int> &label_map){
        string opcode = "1101111";
        string function3 = "";
        string rd = "";
        string immediate20 = j_type[2];
        string binary32 = "";
        string immediate = "";
        if(j_type[0].compare("JAL")==0){
            if(label_map.find(immediate20)!=label_map.end()){
                int line_number = label_map[immediate20];
                immediate20 = integer_hexadecimal_converter(line_number);
                immediate20 = immediate20.substr(immediate20.size()-8);
                immediate20 = "0X"+immediate20;
                immediate20 = immediate_binary_converter(immediate20);
                immediate20 = immediate20.substr(immediate20.size()-20);
                immediate += immediate20[0]+immediate20.substr(10,10)+immediate20[8]+immediate20.substr(1,8);
            }
            else{
                cout<<"\nThe label "<<immediate20<<" is not found.\n";
                return "";
            }
            rd = j_type[1];
            rd = register_binary_converter(rd);
            rd = rd.substr(rd.size()-5);
            if(rd.compare("zzzzz")==0){
                cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
                return "";
            }
            binary32 = immediate+rd+opcode;
            return binary32;
        }
        else{
            cout<<"The instruction is not J type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        return binary32;
    }
    /*converts the instruction into binary for U type instruction*/
    string binary_converter_u_type(vector<string> &u_type){
        string opcode = "";
        if(u_type[0].compare("LUI")==0){
            opcode = "0110111";
        }
        else if(u_type[0].compare("AUIPC")==0){
            opcode = "0010111";
        }
        else{
            cout<<"The instruction is not U type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        string rd = u_type[1];
        rd = register_binary_converter(rd);
        rd = rd.substr(rd.size()-5);
        if(rd.compare("zzzzz")==0){
            cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
            return "";
        }
        string immediate = u_type[2];
        immediate = immediate_binary_converter(immediate);
        immediate = immediate.substr(immediate.size()-8);
        immediate = immediate+"000000000000";
        return immediate+rd+opcode;
    }
    /*converts the instruction into binary for S type instruction*/
    string binary_converter_s_type(vector<string> &s_type){
        string opcode = "0100011";
        string function3 = "";
        string rs1 = s_type[1];
        string rs2 = "";
        string immediate5 = "";
        string immediate7 = "";
        string immediate12 = "";
        string binary32 = "";
        if(s_type[0].compare("SB")==0)
            function3 = "000";
        else if(s_type[0].compare("SH")==0)
            function3 = "001";
        else if(s_type[0].compare("SW")==0)
            function3 = "010";
        else{
            cout<<"The instruction is not S type.\nThere seems to be an error in the assembly instruction.\n"<<endl;
            return "";
        }
        immediate12 = s_type[2];
        rs2 = immediate12.substr(immediate12.find('[')+1);
        rs2 = rs2.substr(0,rs2.size()-1);
        immediate12 = immediate12.substr(0,immediate12.find('['));
        immediate12 = immediate_binary_converter(immediate12);
        immediate12 = immediate12.substr(immediate12.size()-12);
        rs1 = register_binary_converter(rs1);
        rs2 = register_binary_converter(rs2);
        rs1 = rs1.substr(rs1.size()-5);
        rs2 = rs2.substr(rs2.size()-5);
        if(rs1.compare("zzzzz")==0 || rs2.compare("zzzzz")==0){
            cout<<"\nIvalid register entry in instruction.\nGeneral purose registers are named from x0to x31.\n";
            return "";
        }
        immediate5 = immediate12.substr(immediate12.size()-5);
        immediate7 = immediate12.substr(0,7);
        binary32 = immediate7+rs2+rs1+function3+immediate5+opcode;
        return binary32;
    }
    /*final function to convert all assembly instructions into binary
    32bit binary codes are stored in vector<string> for ease of display*/
    vector<string> final_binary_converter(vector<string> &line_tokens,map<string,char> &instruction_map,map<string,int> &label_map){
        char get_instruction_type;
        vector<string> binary;
        string binary32;
        for(auto line:line_tokens){
            vector<string> word_tokens;
            tokenizer(line, word_tokens);
            string first_word = word_tokens[0];
            get_instruction_type = instruction_map[first_word];
            if(word_tokens.size()==1)
                continue;
            switch(get_instruction_type){
                case 'r':
                    binary32 = binary_converter_r_type(word_tokens);
                break;
                case 'i':
                    binary32 = binary_converter_i_type(word_tokens,label_map);
                break;
                case 'b':
                    binary32 = binary_converter_b_type(word_tokens,label_map);
                break;
                case 'j':
                    binary32 = binary_converter_j_type(word_tokens,label_map);
                break;
                case 'u':
                    binary32 = binary_converter_u_type(word_tokens);
                break;
                case 's':
                    binary32 = binary_converter_s_type(word_tokens);
                break;
                default:
                    cout<<"\nWrong instruction"<<endl;
            }
            binary.push_back(binary32);
            binary32 = "";
        }
        return binary;
    }

};

class CPU{
    public:

    class GPR{
        public:
            int ins;
            string value;
            GPR(){
                ins = -1;
                value = "0";
            }
    };

    class Memory{
        public:
            string value;
            Memory(){
                value = "0";
            }
    };


    vector<GPR> gpr;
    vector<Memory> ram;

    /*
    int bin2dec(string binary){
        int decimal = 0;
        bool negative = (binary[0]=='1')?true:false;
        if(negative){
            for(int i=0;i<binary.size();i++)
                binary[i] = (binary[i]=='1')?'0':'1';
            for(int i=0;i<binary.size();i++){
                if(binary[binary.size()-i-1]=='1')  
                    decimal += pow(2,i);
            }
            decimal = -(decimal+1);
        }
        else{
            for(int i=0;i<binary.size();i++){
                if(binary[binary.size()-i-1]=='1')
                    decimal += pow(2,i);
            }
        }
        return decimal;
    }
    */

    int bin2dec(string binary){
        int decimal = 0;
        for(int i=0;i<binary.size();i++)
            if(binary[binary.size()-i-1]=='1')
                decimal += pow(2,i);
        return decimal;
    }
    
    string posdec2bin(int decimal){
        string binary = "";
        while(decimal!=0){
            binary += (decimal%2==0)?"0":"1";
            decimal /= 2;
        }
        reverse(binary.begin(),binary.end());
        return (binary=="")?"0":binary;
    }

    string dec2bin(int decimal){
        const int bits = 32;
        if(decimal==0)
            return string(bits,'0');
        if(decimal>0){
            string binary = posdec2bin(decimal);
            while(binary.size()<bits)
                binary = "0" + binary;
            return binary;
        }
        else{
            int positive = abs(decimal);
            string binary = posdec2bin(positive);
            while(binary.size()<bits)
                binary = "0" + binary;
            for(int i=0;i<bits;i++)
                binary[i] = (binary[i]=='1')?'0':'1';
            bool carry = true;
            for(int i=bits-1;i>=0;i--){
                if(binary[i]=='1' && carry)
                    binary[i] = '0';
                else if(carry){
                    binary[i] = '1';
                    carry = false;
                }
            }
            return binary;
        }
    }


    enum instructionType{
    r_type,
    i_type,
    l_type,
    s_type,
    b_type,
    j_type,
    u_type,
    unknown
    };

    enum ALUSelect{
        ADD = 0b0000,
        SUB = 0b0001,
        AND = 0b0010,
        OR = 0b0011,
        XOR = 0b0100,
        SLL = 0b0101,
        SRL = 0b0110,
        SRA = 0b0111,
        SLT = 0b1000,
        SLTU = 0b1001,
        BEQ = 0b1010,
        BNE = 0b1011,
        BLT = 0b1100,
        BGE = 0b1101,
        BLTU = 0b1110,
        BGEU = 0b1111,
        LUI = 0b10000,
        AUIPC = 0b10001,
        JAL = 0b10010,
        JALR = 0b10011,
        LOAD = 0b10100,
        STORE = 0b10101
    };

    class Control{
    public:
        bool regread;
        bool alusrc;
        bool mem2reg;
        bool regwrite;
        bool memread;
        bool memwrite;
        bool branch;
        bool jump;
        string aluop;
        instructionType decide;
        int ins;
        public:
        Control(){
            regread = true;
            alusrc = false;
            mem2reg = false;
            regwrite = false;
            memread = false;
            memwrite = false;
            branch = false;
            jump = false;
            aluop = "";
            ins = -1;
        }

        void printControl(){
            cout<<"RegRead: "<<regread<<endl;
            cout<<"ALUsrc: "<<alusrc<<endl;
            cout<<"Mem2Reg: "<<mem2reg<<endl;
            cout<<"RegWrite: "<<regwrite<<endl;
            cout<<"MemRead: "<<memread<<endl;
            cout<<"MemWrite: "<<memwrite<<endl;
            cout<<"Branch: "<<branch<<endl;
            cout<<"Jump: "<<jump<<endl;
            cout<<"ALUop: "<<aluop<<endl;
            cout<<"Decide: "<<decide<<endl;
        }

        instructionType getInstructionType(string &opcode){
            if(opcode=="0110011")   return r_type;
            if(opcode=="0010011")   return i_type;
            if(opcode=="0000011")   return l_type;
            if(opcode=="0100011")   return s_type;
            if(opcode=="1100011")   return b_type;
            if(opcode=="0110111")   return u_type;
            if(opcode=="0010111")   return u_type;
            if(opcode=="1101111")   return j_type;
            if(opcode=="1100111")   return j_type;
            return unknown;
        }

        void genControlWord(string &opcode){
            instructionType type = getInstructionType(opcode);
            decide = type;
            cout<<"\n\nInstruction type: "<<type<<"\n\n"<<endl;
            switch(type){
                case r_type:
                    Control();
                    regwrite = true;
                    alusrc = false;
                    aluop = "000";
                    break;
                    break;
                case i_type:
                    regwrite = true;
                    alusrc = true;
                    aluop = "001";
                    break;
                case l_type:
                    regwrite = true;
                    memread = true;
                    alusrc = true;
                    aluop = "010";
                    break;
                case s_type:
                    memwrite = true;
                    alusrc = true;
                    aluop = "011";
                    break;
                case b_type:
                    branch = true;
                    aluop = "100";
                    break;
                case u_type:
                    regwrite = true;
                    alusrc = true;
                    aluop = "101";
                    break;
                case j_type:
                    regwrite = true;
                    jump = true;
                    alusrc = true;
                    aluop = "110";
                    break;
                default:
                    aluop = "ERROR";
                    break;
            }
        }   
    };

    class ALUControl{
    public:
        ALUSelect genALUSelect(string &aluop,string &func3,string &func7,bool &branch,bool &jump){
            if(aluop=="000"){
                if(func3=="000")    return(func7=="0000000")?ADD:SUB;
                else if(func3=="111")   return AND;
                else if(func3=="110")   return OR;
                else if(func3=="100")   return XOR;
                else if(func3=="001")   return SLL;
                else if(func3=="101")   return(func7=="0000000")?SRL:SRA;
                else if(func3=="010")   return SLT;
                else if(func3=="011")   return SLTU;
            }
            else if(aluop=="001"){
                if(func3=="000")    return ADD;
                else if(func3=="111")   return AND;
                else if(func3=="110")   return OR;
                else if(func3=="100")   return XOR;
                else if(func3=="001")   return SLL;
                else if(func3=="101")   return(func7=="0000000")?SRL:SRA;
                else if(func3=="010")   return SLT;
                else if(func3=="011")   return SLTU;
            }
            else if(aluop=="010")   return ADD;
            else if(aluop=="011")   return STORE;
            else if(aluop=="100"){
                if(branch){
                    if(func3=="000")    return BEQ;
                    if(func3=="001")    return BNE;
                    if(func3=="100")    return BLT;
                    if(func3=="101")    return BGE;
                    if(func3=="110")    return BLTU;
                    if(func3=="111")    return BGEU;
                }
            }
            else if(aluop=="101"){

            }
            else if(aluop=="110"){

            }
        }
    };

    class ALU{
    public:
        bool aluzeroflag;

        ALU(){
            aluzeroflag = false;
        }
        
        string ALUOperation(ALUSelect aluselect,string &x,string &y,bool &branch,int imm,CPU &cpu){
            int answer;
            branch = false;
            int a = stoi(x);
            int b = stoi(y);
            switch(aluselect){
                case ADD:
                    answer = a+b;
                    break;
                case SUB:
                    answer = a-b;
                    break;
                case AND:
                    answer = a&b;
                    break;
                case OR:
                    answer = a|b;
                    break;
                case XOR:
                    answer = a^b;
                    break;
                case SLL:
                    answer = a<<b;
                    break;
                case SRL:
                    answer = static_cast<unsigned int>(a)>>b;
                    break;
                case SRA:
                    answer = a>>b;
                    break;
                case SLT:
                    answer = a<b?1:0;
                    break;
                case SLTU:
                    answer = static_cast<unsigned int>(a)<static_cast<unsigned int>(b)?1:0;
                    break;
                case BEQ:
                    aluzeroflag = a==b;
                    answer = 0;
                    break;
                case BNE:
                    aluzeroflag = a!=b;
                    answer = 0;
                    break;
                case BLT:
                    aluzeroflag = a<b;
                    answer = 0;
                    break;
                case BGE:
                    aluzeroflag = a>=b;
                    answer = 0;
                    break;
                case BLTU:
                    aluzeroflag = static_cast<unsigned int>(a)<static_cast<unsigned int>(b);
                    answer = 0;
                    break;
                case BGEU:
                    aluzeroflag = static_cast<unsigned int>(a)>=static_cast<unsigned int>(b);;
                    answer = 0;
                    break;
                default:
                    cout<<"\nALU Operation Error!!!\n"<<endl;
                    break;
                case STORE:
                    answer = a+imm;
                    break;
            }
            return to_string(answer);
        }
    };

    class PC{
    public:
        string value;
        bool valid;
        bool stall;
        PC(){
            value = "";
            valid = false;
            stall = false;
        }
        void print(){
            cout<<underline<<"\nPROGRAM COUNTER\n"<<closeunderline<<endl;
            cout<<"value: "<<value<<endl;
            cout<<"valid: "<<valid<<endl;
            cout<<"stall: "<<stall<<endl;
        }
    };

    class IFID{
        public:
        string dpc;
        string npc;
        string ir;
        bool valid;
        bool stall;
        IFID(){
            dpc = "";
            npc = "";
            ir = "";
            valid = false;
            stall = false;
        }
        void print(){
            cout<<underline<<"\nINSTRUCTION FETCH INSTRUCTION DECODE\n"<<closeunderline<<endl;
            cout<<"dpc: "<<dpc<<endl;
            cout<<"npc: "<<npc<<endl;
            cout<<"ir: "<<ir<<endl;
            cout<<"valid: "<<valid<<endl;
            cout<<"stall: "<<stall<<endl;
        }
    };

    class IDEX{
        public:
        string dpc;
        string jpc;
        string immu;
        string rs1;
        string rs2;
        string imml;
        string func3;
        string func7;
        string rdl;
        bool valid;
        bool stall;
        Control cw;
        IDEX(){
            dpc = "";
            jpc = "";
            immu = "";
            rs1 = "";
            rs2 = "";
            imml = "";
            func3 = "";
            func7 = "";
            rdl = "";
            valid = false;
            stall = false;
        }
        void print(){
            cout<<underline<<"\nINSTRUCTION DECODE EXECUTE\n"<<closeunderline<<endl;
            cout<<"dpc: "<<dpc<<endl;
            cout<<"jpc: "<<jpc<<endl;
            cout<<"immu: "<<immu<<endl;
            cout<<"rs1: "<<rs1<<endl;
            cout<<"rs2: "<<rs2<<endl;
            cout<<"imml: "<<imml<<endl;
            cout<<"func3: "<<func3<<endl;
            cout<<"func7: "<<func7<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cout<<"stall: "<<stall<<endl;
            cw.printControl();
        }
    };
    
    class EXMO{
        public:
        string aluout;
        string rs2;
        string rdl;
        bool valid;
        bool stall;
        Control cw;
        EXMO(){
            aluout = "";
            rs2 = "";
            rdl = "";
            valid = false;
            stall = false;
        }
        void print(){
            cout<<underline<<"\nEXECUTE MEMORY OPERATION\n"<<closeunderline<<endl;
            cout<<"aluout: "<<aluout<<endl;
            cout<<"rs2: "<<rs2<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cout<<"stall: "<<stall<<endl;
            cw.printControl();
        }
    };
    
    class MOWB{
        public:
        string ldout;
        string aluout;
        string rdl;
        bool valid;
        bool stall;
        Control cw;
        MOWB(){
            ldout = "";
            aluout = "";
            rdl = "";
            valid = false;
            stall = false;
        }
        void print(){
            cout<<underline<<"\nMEMORY OPERATION WRITE BACK\n"<<closeunderline<<endl;
            cout<<"ldout: "<<ldout<<endl;
            cout<<"aluout: "<<aluout<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cout<<"stall: "<<stall<<endl;
            cw.printControl();
        }
    };

    class IM{
        public:
        vector<string> ins;
    };

    CPU():ram(64), gpr(32){};

    void printGPR(){
        cout<<underline<<"\nREGISTER FILE\n"<<closeunderline<<endl;
        for(int i=0;i<16;i++){
            cout<<"Register "<<i<<": "<<gpr[i].value;
            cout<<"\t\t\t\tRegister "<<(i+16)<<": "<<gpr[i+16].value<<endl;
        }
    }

    void printRAM(){
        cout<<underline<<"\nDATA MEMORY\n"<<closeunderline<<endl;
        for(int i=0;i<16;i++){
            cout<<"Address "<<(i*4)<<": "<<ram[i].value;
            cout<<"\t\tAddress "<<(i+16)*4<<": "<<ram[i].value;
            cout<<"\t\tAddress "<<(i+32)*4<<": "<<ram[i].value;
            cout<<"\t\tAddress "<<(i+48)*4<<": "<<ram[i].value<<endl;
        }
    }

    void fetch(IFID &ifid,IM &im,PC &pc){
        if(ifid.stall || !pc.valid)
            return;
        ifid.ir = im.ins[stoi(pc.value)];
        ifid.dpc = pc.value;
        ifid.npc = to_string(stoi(pc.value)+1);
        ifid.valid = true;
        pc.value = to_string(stoi(pc.value)+1);
        gpr[0].value = "0";
        if(stoi(pc.value)>=im.ins.size())
            pc.valid = false;
    }

    void decode(CPU &cpu,IDEX &idex,IFID &ifid,PC &pc,Control &control){
        if(idex.stall || !ifid.valid)
            return;
        gpr[0].value = "0";
        idex.jpc = ifid.npc.substr(0,4)+(to_string((stoi(ifid.ir,nullptr,2)&0x3FFFFFF)<<2));
        idex.dpc = ifid.dpc;
        idex.immu = ifid.ir.substr(0,7)+ifid.ir.substr(20,5);
        idex.imml = ifid.ir.substr(0,12);
        idex.func3 = ifid.ir.substr(17,3);
        idex.func7 = ifid.ir.substr(0,7);
        idex.rdl = ifid.ir.substr(20,5);
        string opcode = ifid.ir.substr(25);
        idex.cw.genControlWord(opcode);
        int i1 = cpu.bin2dec(ifid.ir.substr(12,5));
        int i2 = cpu.bin2dec(ifid.ir.substr(7,5));


        if(idex.cw.regread){
            if(gpr[i1].ins==-1){
                if(idex.cw.decide==s_type)
                    idex.rs1 = gpr[i2].value;
                else
                    idex.rs1 = gpr[i1].value;
            }
            else{
                ifid.stall = true;
                return;
            }
        }
        
        if(idex.cw.alusrc){
            if(idex.cw.regread){
                int val1 = cpu.bin2dec(idex.immu);
                int val2 = cpu.bin2dec(idex.imml);
                if(idex.cw.decide==s_type)
                    idex.rs2 = val1;
                else{
                    idex.rs2 = val2;
                }
                
            }
        }
        else{
            if(idex.cw.regread){
                if(gpr[i2].ins==-1)
                    idex.rs2 = gpr[i2].value;
                else{
                    ifid.stall = true;
                    return;
                }
            }
        }
        gpr[stoi(ifid.ir.substr(20,5))].ins = idex.cw.ins;
        ifid.stall = false;
        idex.valid = true;
        gpr[0].value = "0";
        if(!pc.valid)
            ifid.valid = false;
    }

    void execute(CPU &cpu,IFID &ifid,IDEX &idex,EXMO &exmo,ALU &alu,ALUControl &alucontrol,PC &pc){
        gpr[0].value = "0";
        if(exmo.stall || !idex.valid)
            return;
        ALUSelect aluselect = alucontrol.genALUSelect(idex.cw.aluop,idex.func3,idex.func7,idex.cw.branch,idex.cw.jump);
        exmo.aluout = alu.ALUOperation(aluselect,idex.rs1,idex.rs2,idex.cw.branch,cpu.bin2dec(idex.immu),cpu);
        exmo.cw = idex.cw;
        exmo.rs2 = idex.rs2;
        exmo.rdl = idex.rdl;
        if(idex.cw.branch){
            if(alu.aluzeroflag){
                pc.value = (stoi(idex.immu)<<1)+stoi(idex.dpc);
                ifid.valid = false;
                idex.valid = false;
            }
        }
        if(idex.cw.jump){
            pc.value = stoi(idex.jpc);
            ifid.valid = false;
            idex.valid=  false;
            pc.valid = false;
        }
        idex.stall = false;
        exmo.valid = true;
        if(!ifid.valid)
            idex.valid = false;
    }

    void memoryOp(MOWB &mowb,EXMO &exmo,IDEX &idex,PC &pc){
        if(mowb.stall || !exmo.valid)
            return;
        if(stoi(exmo.aluout)>=0 && stoi(exmo.aluout)<ram.size()){
            if(exmo.cw.memwrite)
                ram[stoi(exmo.aluout)].value = idex.rs2;
            if(exmo.cw.memread)
                mowb.ldout = ram[stoi(exmo.aluout)].value;
        }
        mowb.aluout = exmo.aluout;
        mowb.cw = exmo.cw;
        mowb.rdl = exmo.rdl;
        if(idex.cw.branch || idex.cw.jump){
            pc.value = exmo.aluout;
            pc.valid = true;
        }
        exmo.stall = false;
        mowb.valid = true;
        gpr[0].value = "0";
        if(!idex.valid)
            exmo.valid = false;
    }

    void writeBack(CPU &cpu,MOWB &mowb,EXMO &exmo){
        if(!mowb.valid || !mowb.cw.regwrite)
            return;
        int index = cpu.bin2dec(mowb.rdl);
        if(mowb.cw.mem2reg)
            gpr[index].value = mowb.ldout;
        else
            gpr[index].value = mowb.aluout;
        gpr[index].ins = -1;
        mowb.stall = false;
        gpr[0].value = "0";
        if(!exmo.valid)
            mowb.valid = false;
    }

    void runCPU(CPU &cpu,vector<string> &code){
        PC pc;
        IM im;
        im.ins = code;
        IFID ifid;
        IDEX idex;
        EXMO exmo;
        MOWB mowb;
        Control control;
        ALUControl alucontrol;
        ALU alu;
        pc.value = "0";
        pc.valid = true;
        int counter = 0;
        /*
        for(int i=0;i<5;i++){
            cout<<underline<<"\nPASS: "<<counter<<closeunderline<<"\n"<<endl;
            counter++;
            writeBack(mowb);
            memoryOp(mowb,exmo,idex,pc);
            execute(ifid,idex,exmo,alu,alucontrol,pc);
            decode(ifid,idex,control);
            fetch(ifid,im,pc);
            mowb.print();
            exmo.print();
            idex.print();
            ifid.print();
            pc.print();
        }
        */
        while(true){
            if(!pc.valid && !ifid.valid && !idex.valid && !exmo.valid && !mowb.valid)
                break;
            cout<<underline<<"\nPASS: "<<counter+1<<closeunderline<<"\n"<<endl;
            counter++;
            writeBack(cpu,mowb,exmo);
            memoryOp(mowb,exmo,idex,pc);
            execute(cpu,ifid,idex,exmo,alu,alucontrol,pc);
            decode(cpu,idex,ifid,pc,control);
            fetch(ifid,im,pc);
            mowb.print();
            exmo.print();
            idex.print();
            ifid.print();
            pc.print();
        }
        cout<<underline<<"\nEND OF CPU OPERATION\n"<<closeunderline<<endl;
        printGPR();
        printRAM();
    }
};

int main(){
        cout<<underline<<"\n\n\nATHARVA ATUL PENKAR\n22CS02011\nCOMPUTER ORGANIZATION AND ARCHITECTURE LABORATORY\nRISCV ASSEMBLER CODE"<<closeunderline<<endl;
        cout<<underline<<"\nRULES FOR WRITING ASSEMBLY INSTRUCTIONS\n"<<closeunderline<<endl;
        cout<<"1.\tAll instructions should be written in a single line"<<endl;
        cout<<"2.\tAll instructions should be seperated by semi-colon(;)"<<endl;
        cout<<"3.\tAll Labels should be written in the following format:"<<endl;
        cout<<"\t(i)\tloop:;\n\t(ii)\tL2:;"<<endl;
        cout<<"4.\tAll immediate values should be written in the hexadecimal format (0x00000000). Negative values should be accounted for."<<endl;
        cout<<"0xF will not work. Correct instruction is 0x0000000F"<<endl;
        cout<<"\t0xFFFFFFFE = -2"<<endl;
        cout<<"5.\tAll general purpose registers should be written as x0 to x31"<<endl;
        cout<<"6.\tOnly labels should be used in jump and branch statements. Given are following examples:"<<endl;
        cout<<"\t(i)\tbeq x1,x2,loop;\n\t(ii)\tjal x1,label;"<<endl;
        cout<<"7.\tMemory values should be addressed as immediate[register]. Example 0x00000000[x5];"<<endl;
        Assembly assm;
        string assembly_input;
        cout<<underline<<"\nENTER ASSEMBLY INSTRUCTIONS:\n"<<closeunderline<<endl;
        getline(cin,assembly_input);
        assm.toUpperCase(assembly_input);
        vector<string> assembly_tokens;
        assm.tokenizer(assembly_input,assembly_tokens);
        vector<string> line_tokens;
        assm.linebreaker(assembly_input,line_tokens);
        cout<<endl;
        cout<<underline<<"\nLINE TOKENS:\n"<<closeunderline<<endl;
        for(auto line:line_tokens)
            cout<<line<<endl;
        cout<<endl;
        map<string,char> instruction_map;
        assm.instruction_mapper(instruction_map);
        map<string,int> label_map;
        assm.label_mapper(line_tokens,label_map);
        vector<string> machine_code = assm.final_binary_converter(line_tokens,instruction_map,label_map);
        cout<<underline<<"\nBINARY CODE:\n"<<closeunderline<<endl;
        for(auto i:machine_code)
            cout<<i<<endl;
        
        CPU cpu;
        cpu.runCPU(cpu,machine_code);
        return 0;
}
/*
add r1,r2,r3;xori r1,r2,0xFFFFFFF6;L1:;jal r1,l1;jalr r1,r2,l1;sw r1,0x0000000F[r2];
0000000 00011 00010 000 00001 0110011
000000001010 00010 100 00001 0010011
00000000000000000010 00001 1101111
000000000010 00010 000 00001 1100111
0000000 00010 00001 010 01111 0100011

bge r7,r31,l1;add r1,r2,r3;add r1,r2,r3;l1:;
10000001111100111101100001100011
00000000001100010000000010110011
00000000001100010000000010110011

jal r31,l1;add r1,r2,r3;add r1,r2,r3;l1:;
10000000000000000001 11111 1101111
00000000001100010000000010110011
00000000001100010000000010110011

addi x1,x0,0x00000004;addi x2,x0,0x0000000B;addi x3,x0,0x0000000A;addi x4,x0,0x000000FF;add x5,x1,x2;

addi x1,x0,ox0000008;addi x2,x0,0x0000000C;addi x3,x0,0x0000001F;sw x4,0x00000000[x1];

addi x1,x0,0x0000000B;
*/