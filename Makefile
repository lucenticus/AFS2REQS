afs2reqs:  	parser-afs.l parser-afs.y parser.c parser.h
		flex parser-afs.l
		bison -d -v parser-afs.y	
		cc -o $@ parser-afs.tab.c -lfl -g