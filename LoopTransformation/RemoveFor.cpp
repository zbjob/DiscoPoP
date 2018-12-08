#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <cstdlib>
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

using namespace clang;
using namespace std;
using namespace llvm;
// RecursiveASTVisitor is is the big-kahuna visitor that traverses
// everything in the AST.
class MyRecursiveASTVisitor
: public RecursiveASTVisitor<MyRecursiveASTVisitor>
{
	
public:
	MyRecursiveASTVisitor(Rewriter &R, ASTContext &C, set<int> &LineNO) : Rewrite(R),AstContext(C),TransForStmtLineNO(LineNO),flag(true) { }
	
	bool VisitStmt(Stmt *s);
	bool getLoopLB_BO (Expr *E, Stmt *forInit);			
	
	Rewriter &Rewrite;
	ASTContext &AstContext;
	string loopUB,loopUBType,loopUBName;
	string loopLB,loopLBType,loopLBName,loopLBStmt;
	string loopStride;
	string loopBody;
	set<int> &TransForStmtLineNO;
    bool flag;
};

//find the loop lower bound when the loop Init is expression other than Decl
bool MyRecursiveASTVisitor::getLoopLB_BO (Expr *E, Stmt *forInit) {
	if(!isa<BinaryOperator>(E))
		return false;
	BinaryOperator *B = cast<BinaryOperator> (E);
	Expr *LHS = B->getLHS();
	Expr *RHS = B->getRHS();
    
    cout << "now in getLoopLB_BO, forInit is BinaryOperator " << endl;
	if(B->isAssignmentOp()) {
		string tempType = B->getLHS()->getType().getAsString(); 
		string tempName = Rewrite.ConvertToString(B->getLHS()); 
		if( /*tempType == loopUBType && */  tempName == loopUBName) {
			loopLBStmt = Rewrite.ConvertToString(forInit)+ ";";
			loopLBStmt.push_back('\n');
			loopLBName = tempName;
			loopLBType = tempType;
			cout << "************** findLB" << endl;
			return true;
		}
		else
			return false;
	}
	else if (B->getOpcode() == BO_Comma) {
		return getLoopLB_BO(LHS,forInit) || getLoopLB_BO(RHS,forInit);
	}
	else
		return false;
}

// Override Statements which includes expressions and more
bool MyRecursiveASTVisitor::VisitStmt(Stmt *s)
{	
   //Do not parse the included file 
   SourceManager *SM = &(AstContext.getSourceManager());
   SourceLocation ExpansionLoc = SM->getExpansionLoc(s->getLocStart());
   SourceLocation SpellingLoc = SM->getSpellingLoc(ExpansionLoc);
   PresumedLoc PLoc = SM->getPresumedLoc(SpellingLoc);
   //if(!SM->isInMainFile(SpellingLoc)) {
   if(!SM->isFromMainFile(SpellingLoc)) {
       cout << " not in the main file " << endl;
       cout << Rewrite.ConvertToString(s) << endl;
       cout << PLoc.getFilename() << endl;
       return false;
   }
   
    int initTypeFlag = 0; // if = 1, forInit is a binary operator; if = 2, forInit is a  Decleration statement

	int LineNO = AstContext.getFullLoc(s->getLocStart()).getSpellingLineNumber();

	//set<int>::iterator iter = TransForStmtLineNO.find(LineNO);
	if (isa<ForStmt>(s) && TransForStmtLineNO.find(LineNO) != TransForStmtLineNO.end())
	{
		ForStmt *For = cast<ForStmt>(s);
	    
        // get loop upper bound from forCondition statment
		Stmt *forCond = For->getCond();
		if(isa<BinaryOperator>(forCond)) {
			BinaryOperator *E = cast<BinaryOperator> (forCond);
		    if(E->isComparisonOp()) {
				loopUBType = E->getLHS()->getType().getAsString();
				loopUBName = Rewrite.ConvertToString(E->getLHS());
				switch(E->getOpcode()) {
					case BO_NE:
					case BO_LT:
					case BO_GT:
						loopUB = Rewrite.ConvertToString( E->getRHS());
						break;
					case BO_LE:
						loopUB = Rewrite.ConvertToString( E->getRHS()) + "+1";
						break;
					case BO_GE:
						loopUB = Rewrite.ConvertToString( E->getRHS()) + "-1";
						break;
					default:
						flag = false;
						break;
				}
				
			}
			else 
				flag = false;
	    }
		else
			flag = false;
		
		if(flag) {//print out for test.
			cout << "loopUB ************************ " << loopUB << endl;
			cout << "loopUBName ******************** " << loopUBName << endl;
			cout << "loopUBType ******************** " << loopUBType << endl;
		}
		
        //get loop lower bound from the loopIint statment
		Stmt *forInit = For->getInit(); 
		if(isa<BinaryOperator>(forInit)) { 
            initTypeFlag = 1;
			Expr *E = cast<Expr> (forInit);
			flag = getLoopLB_BO(E,forInit);
		}
		else if (isa<DeclStmt> (forInit)) {
            initTypeFlag = 2;
			DeclStmt * Dstmt = cast<DeclStmt> (forInit);	
            cout << "in isa<DeclStmt> (forInit) " << endl;
			int cnt = 1;
			for(DeclStmt::decl_iterator iter = Dstmt->decl_begin() ; iter != Dstmt->decl_end(); ++iter,++cnt) {
				if(*iter && isa<VarDecl>(*iter)) {
					VarDecl *varDecl = cast<VarDecl> (*iter);
					string tempName =  varDecl->getDeclName().getAsString();
					string tempType =  varDecl->getType().getAsString();
                    cout << "tempName " << tempName << endl;
                    cout << "temptype " << tempType << endl;
					if(tempName == loopUBName /*&& tempType == loopUBType*/){  //can not use the type info, because the there might be a implicit convertion
						loopLBStmt = Rewrite.ConvertToString(forInit);
                        size_t pos = loopLBStmt.find('=');
                        loopLB = loopLBStmt.substr(pos+1);
                        pos = loopLB.find(';');
                        loopLB.erase(pos);
						loopLBName = tempName;
						loopLBType = tempType;
						cout << " found ********************loopLowerBound!!!!!!!!!!!!!!!!§§§§§" << endl; 
						cout << "iteration No " << cnt << endl;
					}
				}	
			}
			cout << "iteration time " << cnt << endl;
		}
		
		else 
			flag = false;
		
		if(flag) {
			cout << "loopLBStmt ************************ " << loopLBStmt << endl;
			cout << "loopLBName ******************** " << loopLBName << endl;
			cout << "loopLBType ******************** " << loopLBType << endl;
			
			
			
			loopBody = Rewrite.ConvertToString(For->getBody());
            string brangeframe = "for(" + loopUBType + " " +  loopUBName + " = _range.begin();" +  loopUBName + "!= _range.end();" + " ++" + loopUBName + ")" + string("\r\n");  
            if(loopBody.find('{') == string::npos)  // handle the single line loop body without "{ }"
                loopBody = "{" +  loopBody + ";"+ "}";
            string parforBody;
            if(initTypeFlag == 1) {
			    //parforBody = loopLBStmt + "parallel_for( " + loopLBName + ","+ loopUB +	"," + "[&] (" + loopLBType + " " + loopLBName + ")" + loopBody + ");";
			    parforBody = loopLBStmt + "parallel_for( " + "blocked_range<"+loopUBType+">(" + loopLBName +","+ loopUB + ") ," + "[&] (blocked_range<" + loopUBType + "> & _range )" + "{"+string("\r\n") + brangeframe + loopBody + "}" + string("\r\n") +  ");";

            }
            else if(initTypeFlag == 2) {
                if(loopUBType == "long" && (loopLBType == "int" || loopLBType == "long"))
                    loopLB = "long("+loopLB+")";
			    //parforBody =  "parallel_for( " + loopLB + ","+ loopUB +	"," + "[&] (" + loopLBType + " " + loopLBName + ")" + loopBody + ");";
			    parforBody =  string("parallel_for( ") + "blocked_range<"+loopUBType+">(" + loopLB +","+ loopUB + ") ," + "[&] (blocked_range<" + loopUBType + "> & _range )" + "{"+string("\r\n") + brangeframe + loopBody + "}" + string("\r\n") +  ");";

                
            }
			Rewrite.ReplaceText(For->getSourceRange(),parforBody);
			cout << parforBody << endl;
		}
	}	
	
	
	return true; // returning false aborts the traversal
}


class MyASTConsumer : public ASTConsumer
{
public:
	
	MyASTConsumer(Rewriter &Rewrite, ASTContext &Context, set<int> &LineNO) : rv(Rewrite,Context,LineNO) { }
	virtual bool HandleTopLevelDecl(DeclGroupRef d);
	
	MyRecursiveASTVisitor rv;
};


bool MyASTConsumer::HandleTopLevelDecl(DeclGroupRef d)
{
	typedef DeclGroupRef::iterator iter;
	
	for (iter b = d.begin(), e = d.end(); b != e; ++b)
	{
		rv.TraverseDecl(*b);
	}
	
	return true; // keep going
}


int main(int argc, char **argv)
{
	struct stat sb;
	
	if (argc < 2)
	{
		llvm::errs() << "Usage: CIrewriter <options> <filename>\n";
		return 1;
	}
	
	// Get filename
	string fileName(argv[1]);
    string linesFile(argv[2]);
	
	// Make sure it exists
	if (stat(fileName.c_str(), &sb) == -1)
	{
		perror(fileName.c_str());
		exit(EXIT_FAILURE);
	}
	
    set<int> lines;
    string strline;
    ifstream input(linesFile.c_str());
    while(getline(input,strline)) 
        lines.insert(atoi(strline.c_str()));
    cout <<"lines created " <<  lines.size() << endl;
    for(set<int>::iterator iter =  lines.begin(); iter != lines.end(); ++iter)
        cout << *iter << endl;

    /*
    lines.insert(32);

    //lines.insert(430);
    lines.insert(425);
    lines.insert(44);
    lines.insert(127);
    lines.insert(186);
    lines.insert(196);
    lines.insert(236);
    lines.insert(268);
    lines.insert(327);
    lines.insert(336);
    lines.insert(372);
    lines.insert(451);
    lines.insert(521);
    lines.insert(525);
    lines.insert(576);
    lines.insert(586);
    lines.insert(605);
    */

    

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
	
	MyASTConsumer astConsumer(Rewrite,compiler.getASTContext(),lines);
	
	// Convert <file>.c to <file_out>.c
	std::string outName (fileName);
	size_t ext = outName.rfind(".");
	if (ext == std::string::npos)
		ext = outName.length();
	outName.insert(ext, "_out");
	
	llvm::errs() << "Output to: " << outName << "\n";
	std::string OutErrorInfo;
	//llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo, llvm::sys::fs::F_None);
	llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo, 0);
    cout << "&&&&&&&&&&&&&& line No.372 " << endl;	
	if (OutErrorInfo.empty())
	{
		// Parse the AST
		ParseAST(compiler.getPreprocessor(), &astConsumer, compiler.getASTContext());
		compiler.getDiagnosticClient().EndSourceFile();
		
		// Output some #ifdefs
		outFile << "#include \"tbb/parallel_for.h\" \n";
		outFile << "#include \"tbb/task_scheduler_init.h\"\n";
		outFile << "#include \"tbb/blocked_range.h\"\n";
		outFile << "using namespace tbb; \n";
		llvm::outs()<< "#define L_AND(a, b) a && b\n";
		//	outFile << "#define L_OR(a, b) a || b\n\n";
		cout << "&&&&&&&&&&&&& Line No.384" << endl;
		
		// Now output rewritten source code
		const RewriteBuffer *RewriteBuf =
		Rewrite.getRewriteBufferFor(compiler.getSourceManager().getMainFileID());
		cout << "&&&&&&&&&&&& Line No.386" << endl;
		outFile << string(RewriteBuf->begin(), RewriteBuf->end());
		//cout << string(RewriteBuf->begin(), RewriteBuf->end()) << endl;
		cout << "&&&&&&&&&&&& Line No.388" << endl;
	}
	else
	{
		llvm::errs() << "Cannot open " << outName << " for writing\n";
	}
	
	outFile.close();
	
	return 0;
}
