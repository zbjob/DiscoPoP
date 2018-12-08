#include <set>
#include <vector>
#include <string>
#include <map>
#include <utility>

using namespace std;

typedef int lineNumber;
typedef string variableName; 

class NodeInOut   //inputs and outputs data structure
{
public:
	variableName name;
	string type;
	lineNumber lineNo;
	string ID;
	NodeInOut(variableName n,string t,lineNumber l,string id):name(n),type(t),lineNo(l),ID(id) {}
};

class CU2Node 
{
	private:
		string  CUID;
	    set<int>lineNums;
        int lines2check; 
        // vector<pair<int,bool>> CheckLineVec;
        map<int,bool> CheckedLine;

		int inputVarNum;
		//vector< pair<variableName,lineNumber> > inputs;
		vector<NodeInOut> inputs;
		
		int outputVarNum;
		//vector< pair<variableName,lineNumber> > outputs;
		vector<NodeInOut> outputs;
        string nodeBody;
		
		set<int> predecessor;
		set<int> successor;		
        
	public:
        CU2Node() 
        {
            lines2check = 0;
            nodeBody = "";
        }
		void setCUID (string ID) { CUID = ID;}
		string getCUID () { return CUID;}

		void setLineNums (set<int> LNo) { lineNums = LNo;}
		bool addLineNum (int LNo) 
		{
            ++lines2check;
			return (lineNums.insert(LNo)).second;

		}

        bool isAllLinesChecked()
        {
            return lines2check == 1;
        }

        void checkline() { --lines2check;}

		const set<int> & getLineNums() { return lineNums;}	

		bool IsInLineSet(int lineNo)
        {
            set<int>::iterator iter = lineNums.find(lineNo);
            if(iter != lineNums.end())
                return true;
            else
                return false;
        }

        void markCheckedLine()  // set all the flags to be true, which means they have not been handled yet
        {
            for(set<int>::iterator iter = lineNums.begin(); iter != lineNums.end(); ++iter)
            {
                //CheckLineVec.push_back(make_pair(*iter,true));
               CheckedLine.insert(pair<int,bool> (*iter,true));
            }
                        
        }
                
        //const &vector<pair<int,bool>> getCheckedLine() { return CheckLineVec;} 
        map<int,bool> & getCheckedLine() { return CheckedLine;}

		void setInputVarNum(int num) { inputVarNum = num;}
		int  getInputVarNum() { return inputVarNum;}
		void increaseInputNum() { ++inputVarNum;}
		void decreaseInputNum() { --inputVarNum;}
		void addInputItem (variableName varName, string type,lineNumber LineNo, string ID )
		{
			//pair<variableName,lineNumber> item (varName,LineNo);
			NodeInOut item(varName,type,LineNo,ID);
			inputs.push_back(item);
		}
		
		//vector<pair<variableName,lineNumber>> getInputs() { return inputs;}
		vector<NodeInOut> & getInputs() { return inputs;}	
	
		void setOutputVarNum(int num) { outputVarNum = num;}
        int  getOutputVarNum() { return outputVarNum;}
		void increaseOutputNum() { ++outputVarNum;}
		void decreaseOutputNum() { --outputVarNum;}
		void addOutputItem (variableName varName, string type,lineNumber LineNo, string ID)
		{
			//pair<variableName,lineNumber> item (varName,LineNo);
			NodeInOut item(varName,type,LineNo,ID);
			outputs.push_back(item);
		}
		
		//vector<pair<variableName,lineNumber>> getOutputs(){ return outputs;}
		vector<NodeInOut> & getOutputs() {return outputs;}	

        string getNodeBody() { return nodeBody;}
        void setNodeBody(string str) { nodeBody= str;}
        void addNodeBody(string str) { nodeBody += str;}
            
            
	
		void addPredecessor(int PID) { predecessor.insert(PID);}
		set<int> getPredecessor() { return predecessor;}
		void addSuccessor(int SID) { successor.insert(SID);}
		set<int> getSuccessor() { return successor;}
};
