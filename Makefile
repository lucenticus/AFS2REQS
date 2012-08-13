afs2reqs:  	parser-afs.l parser-afs.y
		flex parser-afs.l
		bison -d -v parser-afs.y	
		cc -o $@ parser-afs.tab.c -lfl