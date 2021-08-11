/* Andy's Transputer dissassembler        
 *
 * http://www.wizzy.com/wizzy/dasm.html
 *  ftp://ftp.wizzy.com/wizzy/dasm.zip 
 */

char           *Primaries[] = {"J", "LDLP", "PFIX", "LDNL",
		  "LDC", "LDNLP", "NFIX", "LDL", "ADC", "CALL", "CJ", "AJW",
			       "EQC", "STL", "STNL", "OPR"};

char           *Secondaries[] = {"REV", "LB", "BSUB", "ENDP",
		  "DIFF", "ADD", "GCALL", "IN", "PROD", "GT", "WSUB", "OUT",
		  "SUB", "STARTP", "OUTBYTE", "OUTWORD", "SETERR", "--11--",
	      "RESETCH", "CSUB0", "--14--", "STOPP", "LADD", "STLB", "STHF",
	     "NORM", "LDIV", "LDPI", "STLF", "XDBLE", "LDPRI", "REM", "RET",
	      "LEND", "LDTIMER", "TESTLDS", "TESTLDE", "TESTLDD", "TESTSTS",
		"TESTSTE", "TESTSTD", "TESTERR", "TESTPRANAL", "TIN", "DIV",
	       "TESTHARDCHAN", "DIST", "DISC", "DISS", "LMUL", "NOT", "XOR",
	     "BCNT", "LSHR", "LSHL", "LSUM",
	    "LSUB", "RUNP", "XWORD", "SB", "GAJW", "SAVEL", "SAVEH", "WCNT",
	      "SHR", "SHL", "MINT", "ALT", "ALTWT", "ALTEND", "AND", "ENBT",
	    "ENBC", "ENBS", "MOVE", "OR", "CSNGL", "CCNT1", "TALT", "LDIFF",
	      "STHB", "TALTWT", "SUM", "MUL", "STTIMER", "STOPERR", "CWORD",
	     "CLRHALTERR", "SETHALTERR", "TESTHALTERR", "DUP", "MOVE2DINIT",
	        "MOVE2DALL", "MOVE2DNONZERO", "MOVE2DZERO", "GTU", "--60--",
	       "--61--", "--62--", "UNPACKSN", "--64--", "--65--", "--66--",
	     "--67--", "--68--", "--69--", "--6A--", "--6B--", "POSTNORMSN",
		   "ROUNDSN", "--6E--", "--6F--", "--70--", "LDINF", "FMUL",
		     "CFLERR", "CRCWORD", "CRCBYTE", "BITCNT", "BITREVWORD",
		  "BITREVNBITS", "POPROT", "TIMERDISABLEH", "TIMERDISABLEL",
		  "TIMERENABLEH", "TIMERENABLEL", "LDMEMSTARTVAL", "--7F--",
		  "FPSTTEST", "WSUBDB", "FPLDNLDBI", "FPCHKERR", "FPSTNLDB",
		      "FPLDTEST", "FPLDNLSNI", "FPADD", "FPSTNLSN", "FPSUB",
			 "FPLDNLDB", "FPMUL", "FPDIV", "FPRANGE", "FPLDNLSN",
	     "FPREMFIRST", "FPREMSTEP", "FPNAN", "FPORDERED", "FPNOTFINITE",
	     "FPGT", "FPEQ", "FPI32TOR32", "FPGE", "FPI32TOR64", "--99--",
	         "FPB32TOR64", "FPLG", "FPTESTERR", "FPRTOI32", "FPSTNLI32",
	    "FPLDZEROSN", "FPLDZERODB", "FPINT", "--A2--", "FPDUP", "FPREV",
	     "--A5--", "FPLDNLADDDB", "FPENTRY3", "FPLDNLMULDB", "FPENTRY2",
		"FPLDNLADDSN", "FPENTRY", "FPLDNLMULSN", "--AD--", "--AE--",
	      "--AF--", "SETTIMESLICE", "BREAK", "CLRJ0BREAK", "SETJ0BREAK",
	    "TESTJ0BREAK", "--B5--", "LDFLAGS", "STFLAGS", "XBWORD", "LBX",
	    "CB", "CBU", "INSPHDR", "READBFR", "LDCONF", "STCONF", "LDCNT",
	    "SSUB", "LDTH", "LDCHSTATUS", "INTDIS", "INTENB", "CIR", "SS",
	    "CHANTYPE", "LS", "CIRU", "FPREM", "FPRN", "FPDIVBY2",
		    "FPMULBY2", "FPSQRT", "FPURP", "FPURM", "FPURZ",
		 "FPUR32TOR64", "FPUR64TOR32", "FPUEXPDEC32", "FPUEXPINC32",
		 "FPUABS", "--FC--", "FPUNOROUND", "FPUCHKI32", "FPUCHKI64",
		     "--E0--", "--E1--", "--E2--", "--E3--", "--E4--",
		 "--E5--", "--E6--", "--E7--", "--E8--", "--E9--", "--EA--",
		 "--EB--", "--EC--", "--ED--", "--EE--", "--EF--", "DEVLB",
			     "DEVSB", "DEVLS", "DEVSS", "DEVLW", "DEVSW",
			     "DEVXSWORD", "LSX", "CS" "CSU"};


char            *FpuEntries[] = {"--FPU00--", "FPUSQRTFIRST", "FPUSQRTSTEP", "FPUSQRTLAST",
                                 "FPURP",     "FPURM",        "FPURZ",       "FPUR32TOR64",
                                 "FPUR64TOR32", "FPUEXPDEC32","FPUEXPINC32", "FPUABS",
                                 "--FPU0C--", "FPUNOROUND",   "FPUCHKI32",   "FPUCHKI64",
                                 "--FPU10--", "FPUDIVBY2",    "FPUMULBY2",   "--FPU13--",
                                 "--FPU14--", "--FPU15--",    "--FPU16--",   "--FPU17--",
                                 "--FPU18--", "--FPU19--",    "--FPU1A--",   "--FPU1B--",
                                 "--FPU1C--", "--FPU1D--",    "--FPU1E--",   "--FPU1F--",
                                 "--FPU20--", "--FPU21--",    "FPURN",       "FPUSETERR"};

char           *Negatives[] = {"--00--", "FPSTALL", "FPLDALL", "STSHADOW",
		"LDSHADOW", "TRET", "GOPROT", "SELTH", "SYSCALL", "=60F7=",
		"=60F6=", "WAIT",
		"SIGNAL", "TIMESLICE", "INSERTQUEUE", "SWAPTIMER",
		"SWAPQUEUE", "=61FF=", "STOPCH", "VOUT", "VIN", "=61Fb=",
		"=61FA=", "SWAPBFR", "SETHDR",
		"SETCHMODE", "INITVLCB", "WRITEHDR", "READHDR", "DISG",
		"ENBG", "GRANT", "STMOVE2DINIT", "CAUSEERROR", "=62FE=",
		"UNMKRC",
		"MKRC", "IRDSQ", "ERDSQ", "STRESPTR", "LDRESPTR", "DEVMOVE",
		"ICL", "FDCL", "ICA", "FDCA"};

