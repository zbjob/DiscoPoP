#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <utility>

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "FlowGraphNode.hpp"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
//#include "DiscoPop_parsing.hpp"
//#include "CUGraph.hpp"

using namespace clang;
using namespace std;
using namespace llvm;

static int jnodeNum = 1; // join_node Number

// RecursiveASTVisitor is is the big-kahuna visitor that traverses
// everything in the AST.
string int2string(int i) 
{
    stringstream ss;
    ss << i;
    return ss.str();
}


class MyRecursiveASTVisitor
: public RecursiveASTVisitor<MyRecursiveASTVisitor>
{
	
public:
	MyRecursiveASTVisitor(Rewriter &R, ASTContext &C ) :Rewrite(R),AstContext(C),flag(true),addEdgesFlag(false) { }
     
	CU2Node *FGNode;	
	bool VisitStmt(Stmt *s);
    void setCUNode(CU2Node *node) { FGNode = node;}
    void setAddEdgesFlag(bool bval) { addEdgesFlag = bval;}
	//CU2Node &FGNode;	
	Rewriter &Rewrite;
	ASTContext &AstContext;
    bool flag;
    bool addEdgesFlag;
    vector< pair<string, string> > edges;
    //string nodeBody;
};

bool MyRecursiveASTVisitor::VisitStmt(Stmt *s)
{
    //weather in the main file
    SourceManager *SM = &(AstContext.getSourceManager());
    SourceLocation ExpansionLoc = SM->getExpansionLoc(s->getLocStart());
    SourceLocation SpellingLoc = SM->getSpellingLoc(ExpansionLoc);
    PresumedLoc PLoc = SM->getPresumedLoc(SpellingLoc);
    if(!SM->isInMainFile(SpellingLoc)) {
        return false;
    }

    int lineNO = AstContext.getFullLoc(s->getLocStart()).getSpellingLineNumber();

    string NODE("node");
    string JNODE("joinNode");
    if(lineNO == 11) {
        for(Stmt::child_iterator it = s->child_begin(); it != s->child_end(); ++it){
            if(isa<DeclRefExpr>(*it)) {
               DeclRefExpr  *varDecl = cast<DeclRefExpr> (*it);
                if(varDecl->getNameInfo().getAsString() == "sum") {
                    cout << varDecl->getNameInfo().getAsString() << endl;
                    cout << " la la la 1 " << varDecl->getType().getAsString() << endl;
                }
            }
        }
    }
    if(lineNO == 8) {
        if(isa<DeclStmt>(s)) {
            DeclStmt  *Dstmt = cast<DeclStmt> (s);
               for(DeclStmt::decl_iterator iter = Dstmt->decl_begin() ; iter != Dstmt->decl_end(); ++iter) {
                   if(isa<VarDecl>(*iter)) {
                       VarDecl *varDecl = cast<VarDecl> (*iter);
                       if(varDecl->getDeclName().getAsString() == "square") {
                           cout << " la la la 2 " << varDecl->getDeclName().getAsString() << varDecl->getType().getAsString() << endl;
                       }
                   }
            }
        }
    }

    
    if(FGNode->IsInLineSet(lineNO) && FGNode->getCheckedLine()[lineNO])
    {
        string str = Rewrite.ConvertToString(s);
        string nodeID = FGNode->getCUID(); 

        if(!FGNode->isAllLinesChecked()) //if it is not the last line to be transformed
        {
            FGNode->checkline(); // this line has been checked, decrease the counter
            FGNode->addNodeBody(str);
            FGNode->getCheckedLine()[lineNO] = false;   //mark the flag, do not traverse this line any more

            Rewrite.RemoveText(s->getSourceRange());   //if this line is not the last line in this CU , delete this line
        }
        else   // to handle the last line in the lineNum set
        {
            if(FGNode->getInputVarNum()>=1 && FGNode->getOutputVarNum()>=1) 
            {   
                string strinputs;  //flow graph node predecessor parameter, which contained in the CU2Node::inputs vector
                string stroutputs; //flow graph node seccussor parameter , which contained in the CU2Node::outputs vector 
                for(vector<NodeInOut>::const_iterator iter = FGNode->getInputs().begin(); iter!= FGNode->getInputs().end(); ++iter)
                    strinputs += (iter->type + ",");
                for(vector<NodeInOut>::const_iterator iter = FGNode->getOutputs().begin(); iter!= FGNode->getOutputs().end(); ++iter)
                    stroutputs += (iter->type + ",");
                strinputs.erase(strinputs.find_last_of(","));  
                stroutputs.erase(stroutputs.find_last_of(","));


                FGNode->addNodeBody(str);
                string nodeBody;
                if(FGNode->getInputVarNum() > 1) {
                    //if this node has multiple inputs, make a join_node and replace the depended variables with tuple ports.
                    string jNodeNumStr = int2string(jnodeNum++);
                    //edges.push_back(make_pair(string("joinNode")+jNodeNumStr, string("node")+nodeID); 
                    string jNodeBody = string("join_node< tbb::flow::tuple<") + strinputs + ">, " + "tbb::flow::queueing " + "> " + " joinNode"+ jNodeNumStr + "(g);";
                    string multiInputBody = FGNode->getNodeBody();
                    int cnt = 0; // index to capture the rigth input variable
                    for(vector<NodeInOut>::const_iterator iter = FGNode->getInputs().begin(); iter!= FGNode->getInputs().end(); ++iter,++cnt)
                    {

                        multiInputBody.replace(multiInputBody.find(iter->name),iter->name.length(), string("get<") + int2string(cnt) + ">(tran_tuple)");

                        edges.push_back(make_pair(NODE+iter->ID, string("get<")+ int2string(cnt)+ ">(" + NODE+nodeID+".input_ports())"));

                        FGNode->setNodeBody(multiInputBody);
                    }
                    nodeBody = jNodeBody + '\n' + "function_node< <tbb::flow::tuple< " + strinputs + " >, " + stroutputs + "> node" + nodeID + "(g,serial,[&](tbb::flow::tuple<" + strinputs + "> tran_tuple ){" + '\n' + FGNode->getNodeBody() + '\n' + "return " + FGNode->getOutputs().begin()->name + ";" +  '\n' + "});" + '\n' ;   

                    edges.push_back(make_pair(JNODE+jNodeNumStr,NODE+ nodeID));
                }
                else {
                    nodeBody = "function_node<" + strinputs + "," + stroutputs + "> node" + nodeID + "(g,unlimited,[](int i){" + '\n' + FGNode->getNodeBody() + '\n' + "return " + FGNode->getOutputs().begin()->name + ";" +  '\n' + "});" + '\n' ;   
                    edges.push_back( make_pair(NODE+FGNode->getInputs().begin()->ID, NODE+nodeID));
                }
                FGNode->getCheckedLine()[lineNO] = false;   //mark the flag.

                FGNode->setNodeBody(nodeBody);
                Rewrite.ReplaceText(s->getSourceRange(), nodeBody + '\n');
             }
            if(addEdgesFlag) {
                for(vector<pair<string,string> >::const_iterator iter = edges.begin(); iter != edges.end(); ++iter){
                    string edgeStr = string("\nmake_edge(") + iter->first + " , "+ iter->second + ");"+ '\n' ;
                    Rewrite.InsertTextAfter(s->getLocEnd(),edgeStr);

                }
            }

        }
    }
        

    return true;
}

class MyASTConsumer : public ASTConsumer
{
public:
	
	MyASTConsumer(Rewriter &Rewrite, ASTContext &Context, vector<CU2Node> &Nodes ) : rv(Rewrite,Context),CUNodes(Nodes){ }
	virtual bool HandleTopLevelDecl(DeclGroupRef d);
	
	MyRecursiveASTVisitor rv;
    vector<CU2Node> &CUNodes;
};


bool MyASTConsumer::HandleTopLevelDecl(DeclGroupRef d)
{
	typedef DeclGroupRef::iterator iter;
    //for each CU(outter loop), tranverse the AST to find the lines belong to it(inner loop).
    vector<CU2Node>::iterator vend = CUNodes.end();
	for(vector<CU2Node>::iterator viter = CUNodes.begin(); viter != CUNodes.end(); ++viter)
    {
        if(viter == vend-1){
            rv.setAddEdgesFlag(true);
        }
        viter->markCheckedLine();  //set the flags,they are traversed
        rv.setCUNode(&(*viter));
	    for (iter b = d.begin(), e = d.end(); b != e; ++b)
        {
		    rv.TraverseDecl(*b);
        }   
        rv.setAddEdgesFlag(false);
    }
	return true; // keep going
}

/////another tool test
///////////////////////////

class IdentifyVarTypeVisitor
: public RecursiveASTVisitor<IdentifyVarTypeVisitor> {
public:
    
    ASTContext *Context;
    CU2Node *FGNode;
    
    bool VisitStmt(Stmt *s);
    void setCUNode(CU2Node *node) { FGNode = node;}
    
	explicit IdentifyVarTypeVisitor(ASTContext *Context)
    : Context(Context) {}

	
};

bool IdentifyVarTypeVisitor::VisitStmt(Stmt *s)
{
    //weather in the main file
    SourceManager *SM = &(Context->getSourceManager());
    SourceLocation ExpansionLoc = SM->getExpansionLoc(s->getLocStart());
    SourceLocation SpellingLoc = SM->getSpellingLoc(ExpansionLoc);
    PresumedLoc PLoc = SM->getPresumedLoc(SpellingLoc);
    if(!SM->isInMainFile(SpellingLoc)) {
        return false;
    }
    
    int lineNO = Context->getFullLoc(s->getLocStart()).getSpellingLineNumber();
    
    // for each FGNode, iterate the input and the output elements. if lineNO is the input or output:
    // 1. get the variable name VarName
    // 2. use lineNO and VarName get the VarType from the AST
    // 3. Write this type information to the CUnodes
    // done.

    for(vector<NodeInOut>::iterator InputIter = FGNode->getInputs().begin(); InputIter != FGNode->getInputs().end(); ++InputIter) {
        if (InputIter->lineNo == lineNO) {
          for(Stmt::child_iterator it = s->child_begin(); it != s->child_end(); ++it){
            if(isa<DeclRefExpr>(*it)) {
                DeclRefExpr  *varDecl = cast<DeclRefExpr> (*it);
                    if(varDecl->getNameInfo().getAsString() == InputIter->name) {
                        cout << varDecl->getNameInfo().getAsString() << endl;
                        cout << "Find Type "<< lineNO << varDecl->getType().getAsString() << endl;
                        InputIter->type = varDecl->getType().getAsString();
                    }
                }
            }//for child_iterator

          if(isa<DeclStmt>(s)) {
            DeclStmt  *Dstmt = cast<DeclStmt> (s);
               for(DeclStmt::decl_iterator iter = Dstmt->decl_begin() ; iter != Dstmt->decl_end(); ++iter) {
                   if(isa<VarDecl>(*iter)) {
                       VarDecl *varDecl = cast<VarDecl> (*iter);
                            if(varDecl->getDeclName().getAsString() == InputIter->name) {
                             cout << "Find Type "<< lineNO << varDecl->getDeclName().getAsString() << endl << varDecl->getType().getAsString() << endl;
                             InputIter->type = varDecl->getDeclName().getAsString();
                             }
                     }
                }
            }

        }
    }
    
    for(vector<NodeInOut>::iterator OutputIter = FGNode->getOutputs().begin(); OutputIter != FGNode->getOutputs().end(); ++OutputIter) {
        if (OutputIter->lineNo == lineNO) {
          for(Stmt::child_iterator it = s->child_begin(); it != s->child_end(); ++it){
            if(isa<DeclRefExpr>(*it)) {
                DeclRefExpr  *varDecl = cast<DeclRefExpr> (*it);
                    if(varDecl->getNameInfo().getAsString() == OutputIter->name) {
                        cout << varDecl->getNameInfo().getAsString() << endl;
                        cout << "Find Type "<< lineNO << varDecl->getType().getAsString() << endl;
                        OutputIter->type = varDecl->getType().getAsString();
                    }
                }
            }//for child_iterator

          if(isa<DeclStmt>(s)) {
            DeclStmt  *Dstmt = cast<DeclStmt> (s);
               for(DeclStmt::decl_iterator iter = Dstmt->decl_begin() ; iter != Dstmt->decl_end(); ++iter) {
                   if(isa<VarDecl>(*iter)) {
                       VarDecl *varDecl = cast<VarDecl> (*iter);
                            if(varDecl->getDeclName().getAsString() == OutputIter->name) {
                             cout << "Find Type "<< lineNO << varDecl->getDeclName().getAsString() << endl << varDecl->getType().getAsString() << endl;
                             OutputIter->type = varDecl->getDeclName().getAsString();
                             }
                     }
                }
            }

        }
    }

    return true;
}



class IdentifyVarTypeConsumer : public clang::ASTConsumer {
public:
	explicit IdentifyVarTypeConsumer(ASTContext *Context, vector<CU2Node> & Nodes)
    : Visitor(Context),CUNodes(Nodes) {}
	
	virtual bool HandleTopLevelDecl(DeclGroupRef d);
    /*
	virtual void HandleTranslationUnit(clang::ASTContext &Context) {
		Visitor.TraverseDecl(Context.getTranslationUnitDecl());
	}
    */
private:
	IdentifyVarTypeVisitor Visitor;
    vector<CU2Node> &CUNodes;
};

bool IdentifyVarTypeConsumer::HandleTopLevelDecl(DeclGroupRef d){
    typedef DeclGroupRef::iterator iter;

    for(vector<CU2Node>::iterator viter = CUNodes.begin(); viter != CUNodes.end(); ++viter) {
        Visitor.setCUNode(&(*viter));
        for (iter b = d.begin(), e = d.end(); b != e; ++b)
            Visitor.TraverseDecl(*b);
    }

    return true;
}


class IdentifyVarTypeAction : public clang::ASTFrontendAction {
public:
    void setCUnodes(vector<CU2Node> & Nodes) {
        CUNodes = Nodes;
    }
	virtual clang::ASTConsumer *CreateASTConsumer(
												  clang::CompilerInstance &Compiler, llvm::StringRef InFile ) {
		return new IdentifyVarTypeConsumer(&Compiler.getASTContext(), CUNodes);
	}
private:
    vector<CU2Node> & CUNodes;
};


int main(int argc, char **argv)
{
    
	struct stat sb;
    vector<CU2Node> CUNODE;    
    CU2Node node1;
    CU2Node node2;
    CU2Node node3;
    node1.addLineNum(8);
    node2.addLineNum(9);
    node1.setCUID("1");
    node2.setCUID("2");
    node3.setCUID("3");
    node3.addLineNum(10);
    node3.addLineNum(11);

    node1.addInputItem("i","int",6,"0");
    node2.addInputItem("i","int",6,"0");
    node1.addOutputItem("square","int",10,"3");
    node2.addOutputItem("cube","int",10,"3");
    node3.addInputItem("square","int",8,"1");
    node3.addInputItem("cube","int",9,"2");
    node3.addOutputItem("result","int",11,"3");

    node1.setInputVarNum(1);
    node1.setOutputVarNum(1);
    node2.setInputVarNum(1);
    node2.setOutputVarNum(1);
    node3.setInputVarNum(2);
    node3.setOutputVarNum(1);


    CUNODE.push_back(node1);
    CUNODE.push_back(node2);
    CUNODE.push_back(node3);

	if (argc < 2)
	{
		llvm::errs() << "Usage: CIrewriter <options> <filename>\n";
		return 1;
	}
	
    //test
    clang::tooling::runToolOnCode(new IdentifyVarTypeAction, llvm::StringRef);
	// Get filename
	std::string fileName(argv[argc - 1]);
	
	// Make sure it exists
	if (stat(fileName.c_str(), &sb) == -1)
	{
		perror(fileName.c_str());
		exit(EXIT_FAILURE);
	}
    
    //get CUs
    //map<string,CU> CUs;
    //getCUs("RegionData.xml",CUs);
    //getCUGraph("Temp.txt",CUs);
    //CU graph done
    
    
    
	
	CompilerInstance compiler;
	DiagnosticOptions diagnosticOptions;
	compiler.createDiagnostics();
	//compiler.createDiagnostics(argc, argv);
	
	// Create an invocation that passes any flags to preprocessor
	CompilerInvocation *Invocation = new CompilerInvocation;
	CompilerInvocation::CreateFromArgs(*Invocation, argv + 1, argv + argc,
									   compiler.getDiagnostics());
	compiler.setInvocation(Invocation);
	
	// Set default target triple
	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
	pto->Triple = llvm::sys::getDefaultTargetTriple();
	llvm::IntrusiveRefCntPtr<TargetInfo>
	pti(TargetInfo::CreateTargetInfo(compiler.getDiagnostics(),
									 pto.getPtr()));
	compiler.setTarget(pti.getPtr());
	
	compiler.createFileManager();
	compiler.createSourceManager(compiler.getFileManager());
	
	HeaderSearchOptions &headerSearchOptions = compiler.getHeaderSearchOpts();
	
	// <Warning!!> -- Platform Specific Code lives here
	// This depends on A) that you're running linux and
	// B) that you have the same GCC LIBs installed that
	// I do.
	// Search through Clang itself for something like this,
	// go on, you won't find it. The reason why is Clang
	// has its own versions of std* which are installed under
	// /usr/local/lib/clang/<version>/include/
	// See somewhere around Driver.cpp:77 to see Clang adding
	// its version of the headers to its include path.
	// To see what include paths need to be here, try
	// clang -v -c test.c
	// or clang++ for C++ paths as used below:
	headerSearchOptions.AddPath("/usr/include/c++/4.6",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/include/c++/4.6/x86_64-linux-gnu",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/include/c++/4.6/backward",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/local/include",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/local/lib/clang/3.3/include",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/include/x86_64-linux-gnu",
								clang::frontend::Angled,
								false,
								false);
	headerSearchOptions.AddPath("/usr/include",
								clang::frontend::Angled,
								false,
								false);
	// </Warning!!> -- End of Platform Specific Code
	
	
	// Allow C++ code to get rewritten
	LangOptions langOpts;
	langOpts.GNUMode = 1; 
	langOpts.CXXExceptions = 1; 
	langOpts.RTTI = 1; 
	langOpts.Bool = 1; 
	langOpts.CPlusPlus = 1; 
	Invocation->setLangDefaults(langOpts,
								clang::IK_CXX,
								clang::LangStandard::lang_cxx0x);
	
	compiler.createPreprocessor();
	compiler.getPreprocessorOpts().UsePredefines = false;
	
	compiler.createASTContext();
	
	// Initialize rewriter
	Rewriter Rewrite;
	Rewrite.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
	
	const FileEntry *pFile = compiler.getFileManager().getFile(fileName);
	compiler.getSourceManager().createMainFileID(pFile);
	compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),
												   &compiler.getPreprocessor());
	
	MyASTConsumer astConsumer(Rewrite,compiler.getASTContext(),CUNODE);
	
	// Convert <file>.c to <file_out>.c
	std::string outName (fileName);
	size_t ext = outName.rfind(".");
	if (ext == std::string::npos)
		ext = outName.length();
	outName.insert(ext, "_out");
	
	llvm::errs() << "Output to: " << outName << "\n";
	std::string OutErrorInfo;
	llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo, llvm::sys::fs::F_None);
	if (OutErrorInfo.empty())
	{
		// Parse the AST
		ParseAST(compiler.getPreprocessor(), &astConsumer, compiler.getASTContext());
		compiler.getDiagnosticClient().EndSourceFile();
		
		outFile << "#include \"tbb/flow_graph.h\"\n";
		outFile << "using namespace tbb; \n";
        outFile << "using namespace tbb::flow; \n";
		
		// Now output rewritten source code
		const RewriteBuffer *RewriteBuf =
		Rewrite.getRewriteBufferFor(compiler.getSourceManager().getMainFileID());
		outFile << string(RewriteBuf->begin(), RewriteBuf->end());
	}
	else
	{
		llvm::errs() << "Cannot open " << outName << " for writing\n";
	}
	
	outFile.close();
	

	return 0;
}
