open BestSpaceOpera.txt write
write BestSpaceOpera.txt "Space: the final frontier. "
write BestSpaceOpera.txt "these are the voyages of the Starship Enterprise. "
write BestSpaceOpera.txt "Its continuing mission: to explore strange new worlds, "
write BestSpaceOpera.txt "to seek out new life and new civilizations, "
write BestSpaceOpera.txt "to boldly go where no one has gone before"
lseek BestSpaceOpera.txt 27
write BestSpaceOpera.txt "T"
lseek BestSpaceOpera.txt 56
write BestSpaceOpera.txt "s"
close BestSpaceOpera.txt
open BestSpaceOpera.txt read
read BestSpaceOpera.txt 217
close BestSpaceOpera.txt
