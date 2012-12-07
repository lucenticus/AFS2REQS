
afs2reqs:	parser-afs.l parser-afs.y parser.c parser.h
		flex parser-afs.l
		bison -d -v parser-afs.y	
		cc -o $@ parser-afs.tab.c -lfl -g
clean:		
	rm -rf bin obj
	rm -rf *.output
	rm -rf *.sem
	rm -rf lex.yy.c
	rm -rf parser-afs.tab.*
	rm -rf afs2reqs