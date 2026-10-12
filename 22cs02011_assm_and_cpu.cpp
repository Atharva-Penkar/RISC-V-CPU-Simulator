#include <bits/stdc++.h>
using namespace std;
#define underline "\033[4m"
#define closeunderline "\033[0m"

/*thrown by the assembler on any error in the assembly input*/
class AssemblyError : public runtime_error{
    public:
    using runtime_error::runtime_error;
};

class Assembly{
    public:
    /*converts all assembly instructions to upper case*/
    void toUpperCase(string &assembly_input){
        for(char &c:assembly_input)
            c = toupper(static_cast<unsigned char>(c));
    }
    /*splits the assembly instructions into corresponding string tokens
    example ADD X1, X2,X3; will be split into <"ADD","X1","X2","X3">
    consecutive separators do not produce empty tokens*/
    void tokenizer(const string &assembly_input,vector<string> &assembly_tokens){
        string word = "";
        for(char c:assembly_input){
            if(c==' ' || c=='\t' || c==',' || c==';'){
                if(!word.empty())
                    assembly_tokens.push_back(word);
                word.clear();
            }
            else
                word += c;
        }
        if(!word.empty())
            assembly_tokens.push_back(word);
    }
    /*splits the single line assembly instruction into lines and stores them in vector<string>*/
    void linebreaker(const string &assembler_input,vector<string> &line_tokens){
        string word = "";
        for(char c:assembler_input){
            word += c;
            if(c==';'){
                pushLine(word,line_tokens);
                word.clear();
            }
        }
        pushLine(word,line_tokens);
    }
    /*maps the assembly instruction to the corresponding instruction type (RV32I only)*/
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
        instruction_map["LW"] = 'i';
        instruction_map["LH"] = 'i';
        instruction_map["LB"] = 'i';
        instruction_map["LHU"] = 'i';
        instruction_map["LBU"] = 'i';
        instruction_map["JALR"] = 'i';

        instruction_map["LUI"] = 'u';
        instruction_map["AUIPC"] = 'u';

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
    /*removes a leading "LABEL:" token and returns the label name ("" if there is none)*/
    string split_label(vector<string> &word_tokens){
        if(word_tokens.empty() || word_tokens[0].back()!=':')
            return "";
        string label = word_tokens[0].substr(0,word_tokens[0].size()-1);
        if(label.empty())
            throw AssemblyError("Empty label name.");
        word_tokens.erase(word_tokens.begin());
        return label;
    }
    /*maps the labels in assembly instructions to the index of the instruction that follows them
    lines holding only a label are not counted as instructions*/
    void label_mapper(const vector<string> &line_tokens,map<string,int> &label_map){
        int instruction_index = 0;
        for(const auto &line:line_tokens){
            vector<string> word_tokens;
            tokenizer(line,word_tokens);
            string label = split_label(word_tokens);
            if(!label.empty()){
                if(label_map.count(label))
                    throw AssemblyError("The label "+label+" is defined more than once.");
                label_map[label] = instruction_index;
            }
            if(!word_tokens.empty())
                instruction_index++;
        }
    }
    /*converts register name x0..x31 to its number*/
    uint32_t register_number(const string &reg){
        bool digits = reg.size()>=2 && reg.size()<=3 && all_of(reg.begin()+1,reg.end(),::isdigit);
        if(!digits || reg[0]!='X' || stoi(reg.substr(1))>31)
            throw AssemblyError("Invalid register "+reg+". General purpose registers are named from x0 to x31.");
        return stoi(reg.substr(1));
    }
    /*converts an immediate to an integer
    hexadecimal (0x...) holds up to 8 digits and is read as 32-bit two's complement, so 0xFFFFFFFE = -2
    decimal may carry a sign, e.g. -2*/
    int64_t immediate_value(const string &immediate){
        string s = immediate;
        bool negative = false;
        if(!s.empty() && (s[0]=='-' || s[0]=='+')){
            negative = s[0]=='-';
            s = s.substr(1);
        }
        int64_t value;
        if(s.size()>2 && s[0]=='0' && s[1]=='X'){
            string digits = s.substr(2);
            if(digits.size()>8 || !all_of(digits.begin(),digits.end(),::isxdigit))
                throw AssemblyError("Invalid hexadecimal immediate "+immediate+".");
            value = static_cast<int32_t>(stoul(digits,nullptr,16));
        }
        else{
            if(s.empty() || s.size()>10 || !all_of(s.begin(),s.end(),::isdigit))
                throw AssemblyError("Invalid immediate "+immediate+".");
            value = stoll(s);
        }
        return negative?-value:value;
    }
    /*checks that value fits in a field of the given width (as a signed or unsigned bit pattern)
    and returns its low bits*/
    uint32_t immediate_field(int64_t value,int bits,const string &text){
        if(value < -(1LL<<(bits-1)) || value >= (1LL<<bits))
            throw AssemblyError("Immediate "+text+" does not fit in "+to_string(bits)+" bits.");
        return static_cast<uint32_t>(value) & ((1u<<bits)-1);
    }
    /*splits a memory operand imm[xN] (or imm(xN)) into immediate and base register*/
    void memory_operand(const string &operand,string &immediate,string &reg){
        size_t open = operand.find_first_of("[(");
        if(open==string::npos || open==0 || (operand.back()!=']' && operand.back()!=')'))
            throw AssemblyError("Invalid memory operand "+operand+". Memory values are addressed as immediate[register].");
        immediate = operand.substr(0,open);
        reg = operand.substr(open+1,operand.size()-open-2);
    }
    /*byte offset from the current instruction to a label (or a numeric byte offset)*/
    int64_t pc_relative_offset(const string &target,int current_index,map<string,int> &label_map){
        auto it = label_map.find(target);
        if(it!=label_map.end())
            return static_cast<int64_t>(it->second-current_index)*4;
        if(!isdigit(static_cast<unsigned char>(target[0])) && target[0]!='-' && target[0]!='+')
            throw AssemblyError("The label "+target+" is not found.");
        int64_t offset = immediate_value(target);
        if(offset%2!=0)
            throw AssemblyError("Branch/jump offset "+target+" must be even.");
        return offset;
    }
    void expect_operands(const vector<string> &tokens,size_t operands){
        if(tokens.size()!=operands+1)
            throw AssemblyError(tokens[0]+" expects "+to_string(operands)+" operands.");
    }
    /*converts the instruction into binary for R type instruction*/
    uint32_t binary_converter_r_type(vector<string> &r_type){
        static const map<string,pair<uint32_t,uint32_t>> funct = {      //{function3,function7}
            {"ADD",{0b000,0b0000000}},{"SUB",{0b000,0b0100000}},{"XOR",{0b100,0b0000000}},
            {"OR",{0b110,0b0000000}},{"AND",{0b111,0b0000000}},{"SLL",{0b001,0b0000000}},
            {"SRL",{0b101,0b0000000}},{"SRA",{0b101,0b0100000}},{"SLT",{0b010,0b0000000}},
            {"SLTU",{0b011,0b0000000}}};
        expect_operands(r_type,3);
        uint32_t opcode = 0b0110011;
        uint32_t rd = register_number(r_type[1]);
        uint32_t rs1 = register_number(r_type[2]);
        uint32_t rs2 = register_number(r_type[3]);
        auto f = funct.at(r_type[0]);
        return f.second<<25 | rs2<<20 | rs1<<15 | f.first<<12 | rd<<7 | opcode;
    }
    /*converts the instruction into binary for I type instruction*/
    uint32_t binary_converter_i_type(vector<string> &i_type,map<string,int> &label_map){
        static const map<string,uint32_t> alu = {
            {"ADDI",0b000},{"SLTI",0b010},{"SLTIU",0b011},{"ANDI",0b111},{"ORI",0b110},{"XORI",0b100}};
        static const map<string,pair<uint32_t,uint32_t>> shift = {      //{function3,immediate[11:5]}
            {"SLLI",{0b001,0b0000000}},{"SRLI",{0b101,0b0000000}},{"SRAI",{0b101,0b0100000}}};
        static const map<string,uint32_t> load = {
            {"LB",0b000},{"LH",0b001},{"LW",0b010},{"LBU",0b100},{"LHU",0b101}};
        const string &name = i_type[0];
        uint32_t rd,rs1,immediate12,function3,opcode;
        if(alu.count(name)){
            expect_operands(i_type,3);
            opcode = 0b0010011;
            function3 = alu.at(name);
            rd = register_number(i_type[1]);
            rs1 = register_number(i_type[2]);
            immediate12 = immediate_field(immediate_value(i_type[3]),12,i_type[3]);
        }
        else if(shift.count(name)){
            expect_operands(i_type,3);
            opcode = 0b0010011;
            function3 = shift.at(name).first;
            rd = register_number(i_type[1]);
            rs1 = register_number(i_type[2]);
            int64_t shamt = immediate_value(i_type[3]);
            if(shamt<0 || shamt>31)
                throw AssemblyError("Shift amount "+i_type[3]+" must be between 0 and 31.");
            immediate12 = shift.at(name).second<<5 | static_cast<uint32_t>(shamt);
        }
        else if(load.count(name)){
            expect_operands(i_type,2);
            opcode = 0b0000011;
            function3 = load.at(name);
            rd = register_number(i_type[1]);
            string immediate,base;
            memory_operand(i_type[2],immediate,base);
            rs1 = register_number(base);
            immediate12 = immediate_field(immediate_value(immediate),12,immediate);
        }
        else if(name=="JALR"){
            /*jalr rd,rs1,imm|label  -  jumps to rs1+imm; a label gives its absolute byte address*/
            expect_operands(i_type,3);
            opcode = 0b1100111;
            function3 = 0b000;
            rd = register_number(i_type[1]);
            rs1 = register_number(i_type[2]);
            auto it = label_map.find(i_type[3]);
            int64_t value = (it!=label_map.end())?static_cast<int64_t>(it->second)*4:immediate_value(i_type[3]);
            immediate12 = immediate_field(value,12,i_type[3]);
        }
        else
            throw AssemblyError(name+" is not an I type instruction.");
        return immediate12<<20 | rs1<<15 | function3<<12 | rd<<7 | opcode;
    }
    /*converts the instruction into binary for B type instruction
    the offset is PC-relative in bytes; bits imm[12|10:5] and imm[4:1|11] are encoded*/
    uint32_t binary_converter_b_type(vector<string> &b_type,map<string,int> &label_map,int current_index){
        static const map<string,uint32_t> funct = {
            {"BEQ",0b000},{"BNE",0b001},{"BLT",0b100},{"BGE",0b101},{"BLTU",0b110},{"BGEU",0b111}};
        expect_operands(b_type,3);
        uint32_t opcode = 0b1100011;
        uint32_t rs1 = register_number(b_type[1]);
        uint32_t rs2 = register_number(b_type[2]);
        int64_t offset = pc_relative_offset(b_type[3],current_index,label_map);
        if(offset < -4096 || offset > 4094)
            throw AssemblyError("Branch target "+b_type[3]+" is out of range.");
        uint32_t imm = static_cast<uint32_t>(offset);
        return ((imm>>12)&1)<<31 | ((imm>>5)&0x3F)<<25 | rs2<<20 | rs1<<15 | funct.at(b_type[0])<<12
             | ((imm>>1)&0xF)<<8 | ((imm>>11)&1)<<7 | opcode;
    }
    /*converts the instruction into binary for J type instruction
    the offset is PC-relative in bytes; bits imm[20|10:1|11|19:12] are encoded*/
    uint32_t binary_converter_j_type(vector<string> &j_type,map<string,int> &label_map,int current_index){
        expect_operands(j_type,2);
        uint32_t opcode = 0b1101111;
        uint32_t rd = register_number(j_type[1]);
        int64_t offset = pc_relative_offset(j_type[2],current_index,label_map);
        if(offset < -(1<<20) || offset >= (1<<20))
            throw AssemblyError("Jump target "+j_type[2]+" is out of range.");
        uint32_t imm = static_cast<uint32_t>(offset);
        return ((imm>>20)&1)<<31 | ((imm>>1)&0x3FF)<<21 | ((imm>>11)&1)<<20 | ((imm>>12)&0xFF)<<12
             | rd<<7 | opcode;
    }
    /*converts the instruction into binary for U type instruction; the immediate is the 20-bit upper value*/
    uint32_t binary_converter_u_type(vector<string> &u_type){
        expect_operands(u_type,2);
        uint32_t opcode = (u_type[0]=="LUI")?0b0110111:0b0010111;
        uint32_t rd = register_number(u_type[1]);
        uint32_t immediate20 = immediate_field(immediate_value(u_type[2]),20,u_type[2]);
        return immediate20<<12 | rd<<7 | opcode;
    }
    /*converts the instruction into binary for S type instruction
    sw rs2,imm[rs1]  -  rs2 is the data register, rs1 the base register*/
    uint32_t binary_converter_s_type(vector<string> &s_type){
        static const map<string,uint32_t> funct = {{"SB",0b000},{"SH",0b001},{"SW",0b010}};
        expect_operands(s_type,2);
        uint32_t opcode = 0b0100011;
        uint32_t rs2 = register_number(s_type[1]);
        string immediate,base;
        memory_operand(s_type[2],immediate,base);
        uint32_t rs1 = register_number(base);
        uint32_t immediate12 = immediate_field(immediate_value(immediate),12,immediate);
        return (immediate12>>5)<<25 | rs2<<20 | rs1<<15 | funct.at(s_type[0])<<12 | (immediate12&0x1F)<<7 | opcode;
    }
    /*final function to convert all assembly instructions into binary
    32bit binary codes are stored in vector<string> for ease of display
    throws AssemblyError naming the offending line on any error*/
    vector<string> final_binary_converter(vector<string> &line_tokens,map<string,char> &instruction_map,map<string,int> &label_map){
        vector<string> binary;
        for(auto &line:line_tokens){
            vector<string> word_tokens;
            tokenizer(line, word_tokens);
            split_label(word_tokens);
            if(word_tokens.empty())
                continue;
            int current_index = binary.size();
            try{
                auto it = instruction_map.find(word_tokens[0]);
                if(it==instruction_map.end())
                    throw AssemblyError("Unknown instruction "+word_tokens[0]+".");
                uint32_t binary32 = 0;
                switch(it->second){
                    case 'r':
                        binary32 = binary_converter_r_type(word_tokens);
                    break;
                    case 'i':
                        binary32 = binary_converter_i_type(word_tokens,label_map);
                    break;
                    case 'b':
                        binary32 = binary_converter_b_type(word_tokens,label_map,current_index);
                    break;
                    case 'j':
                        binary32 = binary_converter_j_type(word_tokens,label_map,current_index);
                    break;
                    case 'u':
                        binary32 = binary_converter_u_type(word_tokens);
                    break;
                    case 's':
                        binary32 = binary_converter_s_type(word_tokens);
                    break;
                }
                binary.push_back(bitset<32>(binary32).to_string());
            }
            catch(const AssemblyError &e){
                throw AssemblyError("Error in \""+line+"\": "+e.what());
            }
        }
        return binary;
    }

    private:
    /*adds a trimmed line to line_tokens if it holds anything besides spaces and ';'*/
    void pushLine(const string &line,vector<string> &line_tokens){
        size_t first = line.find_first_not_of(" \t");
        if(first==string::npos || line[first]==';')
            return;
        line_tokens.push_back(line.substr(first));
    }
};

class CPU{
    public:
    static const int MEMORY_WORDS = 64;
    static const int MAX_CYCLES = 10000;

    class GPR{
        public:
            int ins;            //tag of the in-flight instruction that will write this register, -1 if none
            int32_t value;
            GPR(){
                ins = -1;
                value = 0;
            }
    };

    vector<GPR> gpr;
    vector<uint8_t> ram;        //byte addressed, little-endian

    /*converts a binary string to an integer; with sign_extend the leftmost bit is the sign bit*/
    static int32_t bin2dec(const string &binary,bool sign_extend = false){
        uint32_t value = 0;
        for(char c:binary)
            value = (value<<1) | (c=='1');
        size_t bits = binary.size();
        if(sign_extend && bits>0 && bits<32 && binary[0]=='1')
            value |= ~0u<<bits;
        return static_cast<int32_t>(value);
    }

    /*sign extends the low bits of value*/
    static int32_t signExtend(uint32_t value,int bits){
        uint32_t sign = 1u<<(bits-1);
        value &= (sign<<1)-1;
        return static_cast<int32_t>((value^sign)-sign);
    }

    static string dec2bin(int32_t decimal){
        return bitset<32>(static_cast<uint32_t>(decimal)).to_string();
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
        INVALID = 0b11111
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
        string opcode;
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
            opcode = "";
            decide = unknown;
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
            cout<<"Ins: "<<ins<<endl;
        }

        static instructionType getInstructionType(const string &opcode){
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

        /*regread: rs1 is read; rs2 is read when !alusrc (R, B) or for stores (data register)*/
        bool readsRs2(){
            return regread && (!alusrc || memwrite);
        }

        void genControlWord(const string &op){
            *this = Control();
            opcode = op;
            decide = getInstructionType(op);
            switch(decide){
                case r_type:
                    regwrite = true;
                    alusrc = false;
                    aluop = "000";
                    break;
                case i_type:
                    regwrite = true;
                    alusrc = true;
                    aluop = "001";
                    break;
                case l_type:
                    regwrite = true;
                    memread = true;
                    mem2reg = true;
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
                    regread = false;
                    regwrite = true;
                    alusrc = true;
                    aluop = "101";
                    break;
                case j_type:
                    regread = (op=="1100111");      //JALR reads rs1, JAL reads nothing
                    regwrite = true;
                    jump = true;
                    alusrc = true;
                    aluop = "110";
                    break;
                default:
                    regread = false;
                    aluop = "ERROR";
                    break;
            }
        }
    };

    class ALUControl{
    public:
        ALUSelect genALUSelect(const Control &cw,const string &func3,const string &func7){
            const string &aluop = cw.aluop;
            if(aluop=="000" || aluop=="001"){
                if(func3=="000")    return(aluop=="000" && func7=="0100000")?SUB:ADD;
                else if(func3=="111")   return AND;
                else if(func3=="110")   return OR;
                else if(func3=="100")   return XOR;
                else if(func3=="001")   return SLL;
                else if(func3=="101")   return(func7=="0100000")?SRA:SRL;
                else if(func3=="010")   return SLT;
                else if(func3=="011")   return SLTU;
            }
            else if(aluop=="010" || aluop=="011")   return ADD;     //load/store address = rs1+imm
            else if(aluop=="100"){
                if(func3=="000")    return BEQ;
                if(func3=="001")    return BNE;
                if(func3=="100")    return BLT;
                if(func3=="101")    return BGE;
                if(func3=="110")    return BLTU;
                if(func3=="111")    return BGEU;
            }
            else if(aluop=="101")
                return(cw.opcode=="0110111")?LUI:AUIPC;
            else if(aluop=="110")
                return(cw.opcode=="1101111")?JAL:JALR;
            return INVALID;
        }
    };

    class ALU{
    public:
        bool aluzeroflag;       //set when a branch condition holds

        ALU(){
            aluzeroflag = false;
        }

        /*arithmetic is done on uint32_t so that overflow wraps instead of being undefined*/
        int32_t ALUOperation(ALUSelect aluselect,int32_t x,int32_t y,int32_t imm,uint32_t pc){
            uint32_t a = static_cast<uint32_t>(x);
            uint32_t b = static_cast<uint32_t>(y);
            uint32_t shamt = b&31;
            uint32_t answer = 0;
            aluzeroflag = false;
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
                    answer = a<<shamt;
                    break;
                case SRL:
                    answer = a>>shamt;
                    break;
                case SRA:
                    answer = (x<0)?~(~a>>shamt):(a>>shamt);
                    break;
                case SLT:
                    answer = x<y;
                    break;
                case SLTU:
                    answer = a<b;
                    break;
                case BEQ:
                    aluzeroflag = a==b;
                    break;
                case BNE:
                    aluzeroflag = a!=b;
                    break;
                case BLT:
                    aluzeroflag = x<y;
                    break;
                case BGE:
                    aluzeroflag = x>=y;
                    break;
                case BLTU:
                    aluzeroflag = a<b;
                    break;
                case BGEU:
                    aluzeroflag = a>=b;
                    break;
                case LUI:
                    answer = static_cast<uint32_t>(imm);
                    break;
                case AUIPC:
                    answer = pc+static_cast<uint32_t>(imm);
                    break;
                case JAL:
                case JALR:
                    answer = pc+4;      //link address
                    break;
                default:
                    cout<<"\nALU Operation Error!!!\n"<<endl;
                    break;
            }
            return static_cast<int32_t>(answer);
        }
    };

    class PC{
    public:
        uint32_t value;
        bool valid;
        PC(){
            value = 0;
            valid = false;
        }
        void print(){
            cout<<underline<<"\nPROGRAM COUNTER\n"<<closeunderline<<endl;
            cout<<"value: "<<value<<endl;
            cout<<"valid: "<<valid<<endl;
        }
    };

    class IFID{
        public:
        uint32_t dpc;
        uint32_t npc;
        string ir;
        bool valid;
        bool stall;
        IFID(){
            dpc = 0;
            npc = 0;
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
        uint32_t dpc;
        uint32_t jpc;           //branch / JAL target
        int32_t imm;
        int32_t rs1;
        int32_t rs2;
        string func3;
        string func7;
        int rdl;
        bool valid;
        Control cw;
        IDEX(){
            dpc = 0;
            jpc = 0;
            imm = 0;
            rs1 = 0;
            rs2 = 0;
            func3 = "";
            func7 = "";
            rdl = 0;
            valid = false;
        }
        void print(){
            cout<<underline<<"\nINSTRUCTION DECODE EXECUTE\n"<<closeunderline<<endl;
            cout<<"dpc: "<<dpc<<endl;
            cout<<"jpc: "<<jpc<<endl;
            cout<<"imm: "<<imm<<endl;
            cout<<"rs1: "<<rs1<<endl;
            cout<<"rs2: "<<rs2<<endl;
            cout<<"func3: "<<func3<<endl;
            cout<<"func7: "<<func7<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cw.printControl();
        }
    };

    class EXMO{
        public:
        int32_t aluout;
        int32_t rs2;
        string func3;
        int rdl;
        bool valid;
        Control cw;
        EXMO(){
            aluout = 0;
            rs2 = 0;
            func3 = "";
            rdl = 0;
            valid = false;
        }
        void print(){
            cout<<underline<<"\nEXECUTE MEMORY OPERATION\n"<<closeunderline<<endl;
            cout<<"aluout: "<<aluout<<endl;
            cout<<"rs2: "<<rs2<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cw.printControl();
        }
    };

    class MOWB{
        public:
        int32_t ldout;
        int32_t aluout;
        int rdl;
        bool valid;
        Control cw;
        MOWB(){
            ldout = 0;
            aluout = 0;
            rdl = 0;
            valid = false;
        }
        void print(){
            cout<<underline<<"\nMEMORY OPERATION WRITE BACK\n"<<closeunderline<<endl;
            cout<<"ldout: "<<ldout<<endl;
            cout<<"aluout: "<<aluout<<endl;
            cout<<"rdl: "<<rdl<<endl;
            cout<<"valid: "<<valid<<endl;
            cw.printControl();
        }
    };

    class IM{
        public:
        vector<string> ins;
        /*true if pc is a word-aligned address inside the program*/
        bool contains(uint32_t pc){
            return pc%4==0 && pc/4<ins.size();
        }
    };

    CPU():gpr(32), ram(MEMORY_WORDS*4){};

    int32_t readWord(uint32_t address){
        uint32_t word = 0;
        for(int i=3;i>=0;i--)
            word = (word<<8) | ram[address+i];
        return static_cast<int32_t>(word);
    }

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
            for(int column=0;column<4;column++){
                uint32_t address = (i+16*column)*4;
                cout<<"Address "<<address<<": "<<readWord(address)<<((column==3)?"\n":"\t\t");
            }
        }
    }

    /*in every stage a pipeline register is consumed (valid = false) once the next stage has used it;
    stages run in reverse order, so each stage reads its input before the previous stage overwrites it*/
    void fetch(IFID &ifid,IM &im,PC &pc){
        if(ifid.valid || !pc.valid)     //ifid still holds a stalled instruction
            return;
        ifid.ir = im.ins[pc.value/4];
        ifid.dpc = pc.value;
        ifid.npc = pc.value+4;
        ifid.valid = true;
        ifid.stall = false;
        pc.value += 4;
        pc.valid = im.contains(pc.value);
    }

    void decode(IDEX &idex,IFID &ifid,int &instruction_tag){
        if(!ifid.valid)
            return;
        uint32_t ir = static_cast<uint32_t>(bin2dec(ifid.ir));
        Control cw;
        cw.genControlWord(ifid.ir.substr(25));
        if(cw.decide==unknown){
            cout<<"\nUnknown instruction "<<ifid.ir<<" at address "<<ifid.dpc<<" is skipped.\n"<<endl;
            ifid.valid = false;
            return;
        }
        int i1 = (ir>>15)&31;
        int i2 = (ir>>20)&31;
        int rd = (ir>>7)&31;
        /*data hazard: wait until the pending write back of a source register has happened*/
        if((cw.regread && gpr[i1].ins!=-1) || (cw.readsRs2() && gpr[i2].ins!=-1)){
            ifid.stall = true;
            return;
        }
        ifid.stall = false;
        idex.dpc = ifid.dpc;
        idex.func3 = ifid.ir.substr(17,3);
        idex.func7 = ifid.ir.substr(0,7);
        idex.rdl = rd;
        idex.rs1 = cw.regread?gpr[i1].value:0;
        idex.rs2 = cw.readsRs2()?gpr[i2].value:0;
        switch(cw.decide){
            case s_type:
                idex.imm = signExtend((ir>>25)<<5 | ((ir>>7)&0x1F),12);
                break;
            case b_type:
                idex.imm = signExtend(((ir>>31)&1)<<12 | ((ir>>7)&1)<<11 | ((ir>>25)&0x3F)<<5 | ((ir>>8)&0xF)<<1,13);
                break;
            case u_type:
                idex.imm = static_cast<int32_t>(ir&0xFFFFF000);
                break;
            case j_type:
                if(cw.opcode=="1101111")    //JAL
                    idex.imm = signExtend(((ir>>31)&1)<<20 | ((ir>>12)&0xFF)<<12 | ((ir>>20)&1)<<11 | ((ir>>21)&0x3FF)<<1,21);
                else                        //JALR
                    idex.imm = signExtend(ir>>20,12);
                break;
            default:
                idex.imm = signExtend(ir>>20,12);
                break;
        }
        idex.jpc = idex.dpc+static_cast<uint32_t>(idex.imm);
        cw.ins = instruction_tag++;
        if(cw.regwrite && rd!=0)
            gpr[rd].ins = cw.ins;
        idex.cw = cw;
        idex.valid = true;
        ifid.valid = false;
    }

    void execute(IFID &ifid,IDEX &idex,EXMO &exmo,ALU &alu,ALUControl &alucontrol,PC &pc,IM &im){
        if(!idex.valid)
            return;
        ALUSelect aluselect = alucontrol.genALUSelect(idex.cw,idex.func3,idex.func7);
        int32_t operand2 = idex.cw.alusrc?idex.imm:idex.rs2;
        exmo.aluout = alu.ALUOperation(aluselect,idex.rs1,operand2,idex.imm,idex.dpc);
        exmo.cw = idex.cw;
        exmo.rs2 = idex.rs2;
        exmo.func3 = idex.func3;
        exmo.rdl = idex.rdl;
        exmo.valid = true;
        idex.valid = false;
        if((idex.cw.branch && alu.aluzeroflag) || idex.cw.jump){
            uint32_t target = (aluselect==JALR)?((static_cast<uint32_t>(idex.rs1)+idex.imm)&~1u):idex.jpc;
            pc.value = target;
            pc.valid = im.contains(target);
            if(!pc.valid && target!=4*im.ins.size())
                cout<<"\nJump/branch target "<<target<<" is outside the program. Fetching stops.\n"<<endl;
            /*the instruction fetched after the branch is on the wrong path; it has not been decoded yet*/
            ifid.valid = false;
            ifid.stall = false;
        }
    }

    void memoryOp(MOWB &mowb,EXMO &exmo){
        if(!exmo.valid)
            return;
        mowb.ldout = 0;
        if(exmo.cw.memwrite || exmo.cw.memread){
            int func3 = bin2dec(exmo.func3);
            uint32_t width = 1u<<(func3&3);
            uint32_t address = static_cast<uint32_t>(exmo.aluout);
            if(address>=ram.size() || ram.size()-address<width || address%width!=0)
                cout<<"\nInvalid memory access at address "<<address<<" is ignored.\n"<<endl;
            else if(exmo.cw.memwrite){
                for(uint32_t i=0;i<width;i++)
                    ram[address+i] = (static_cast<uint32_t>(exmo.rs2)>>(8*i))&0xFF;
            }
            else{
                uint32_t value = 0;
                for(int i=width-1;i>=0;i--)
                    value = (value<<8) | ram[address+i];
                bool is_unsigned = func3&4;      //LBU, LHU
                mowb.ldout = (is_unsigned || width==4)?static_cast<int32_t>(value):signExtend(value,8*width);
            }
        }
        mowb.aluout = exmo.aluout;
        mowb.cw = exmo.cw;
        mowb.rdl = exmo.rdl;
        mowb.valid = true;
        exmo.valid = false;
    }

    void writeBack(MOWB &mowb){
        if(!mowb.valid)
            return;
        int index = mowb.rdl;
        if(mowb.cw.regwrite && index!=0){
            gpr[index].value = mowb.cw.mem2reg?mowb.ldout:mowb.aluout;
            if(gpr[index].ins==mowb.cw.ins)     //a younger writer may still be in flight
                gpr[index].ins = -1;
        }
        mowb.valid = false;
    }

    void runCPU(vector<string> &code){
        PC pc;
        IM im;
        im.ins = code;
        IFID ifid;
        IDEX idex;
        EXMO exmo;
        MOWB mowb;
        ALUControl alucontrol;
        ALU alu;
        pc.value = 0;
        pc.valid = im.contains(0);
        int counter = 0;
        int instruction_tag = 0;
        while(pc.valid || ifid.valid || idex.valid || exmo.valid || mowb.valid){
            if(counter==MAX_CYCLES){
                cout<<"\nStopped after "<<MAX_CYCLES<<" cycles (possible infinite loop).\n"<<endl;
                break;
            }
            cout<<underline<<"\nPASS: "<<counter+1<<closeunderline<<"\n"<<endl;
            counter++;
            writeBack(mowb);
            memoryOp(mowb,exmo);
            execute(ifid,idex,exmo,alu,alucontrol,pc,im);
            decode(idex,ifid,instruction_tag);
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
        cout<<"\t(i)\tloop:;\n\t(ii)\tL2: add x1,x2,x3;"<<endl;
        cout<<"4.\tImmediate values are written in hexadecimal (0xF, 0x0000000F) or decimal (15, -2)."<<endl;
        cout<<"\tHexadecimal values are 32-bit two's complement: 0xFFFFFFFE = -2"<<endl;
        cout<<"5.\tAll general purpose registers should be written as x0 to x31"<<endl;
        cout<<"6.\tLabels should be used in jump and branch statements. Given are following examples:"<<endl;
        cout<<"\t(i)\tbeq x1,x2,loop;\n\t(ii)\tjal x1,label;"<<endl;
        cout<<"7.\tMemory values should be addressed as immediate[register]. Example 0x00000000[x5];"<<endl;
        cout<<"8.\tlui/auipc take the 20-bit upper immediate, e.g. lui x1,0x12345;"<<endl;
        Assembly assm;
        string assembly_input;
        cout<<underline<<"\nENTER ASSEMBLY INSTRUCTIONS:\n"<<closeunderline<<endl;
        getline(cin,assembly_input);
        assm.toUpperCase(assembly_input);
        vector<string> line_tokens;
        assm.linebreaker(assembly_input,line_tokens);
        cout<<endl;
        cout<<underline<<"\nLINE TOKENS:\n"<<closeunderline<<endl;
        for(auto &line:line_tokens)
            cout<<line<<endl;
        cout<<endl;
        map<string,char> instruction_map;
        assm.instruction_mapper(instruction_map);
        map<string,int> label_map;
        vector<string> machine_code;
        try{
            assm.label_mapper(line_tokens,label_map);
            machine_code = assm.final_binary_converter(line_tokens,instruction_map,label_map);
        }
        catch(const AssemblyError &e){
            cout<<"\n"<<e.what()<<"\nAssembly failed.\n"<<endl;
            return 1;
        }
        cout<<underline<<"\nBINARY CODE:\n"<<closeunderline<<endl;
        for(auto &i:machine_code)
            cout<<i<<endl;

        CPU cpu;
        cpu.runCPU(machine_code);
        return 0;
}
/*
SAMPLE PROGRAMS

add x1,x2,x3;xori x1,x2,0xFFFFFFF6;L1:;jal x1,l1;jalr x1,x2,l1;sw x1,0x0000000F[x2];
00000000001100010000000010110011
11111111011000010100000010010011
00000000000000000000000011101111    (jal to itself: offset 0)
00000000100000010000000011100111    (jalr to address 8)
00000000000100010010011110100011

addi x1,x0,0x00000004;addi x2,x0,0x0000000B;addi x3,x0,0x0000000A;addi x4,x0,0x000000FF;add x5,x1,x2;
x1=4 x2=11 x3=10 x4=255 x5=15

addi x1,x0,8;addi x4,x0,-2;sw x4,0x0[x1];lw x5,0x0[x1];lbu x6,0x0[x1];
mem[8]=-2 x5=-2 x6=254

addi x1,x0,0;addi x2,x0,5;loop:;addi x1,x1,1;bne x1,x2,loop;jal x3,end;addi x4,x0,99;end:;
x1=5 x3=20 x4=0
*/
