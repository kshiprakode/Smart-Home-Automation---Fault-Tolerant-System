mkdir -p output/
cd src/
gcc database.c -o ../output/database -lpthread
gcc gateway.c -o ../output/gateway -lpthread
gcc gateway.c -o ../output/gateway -lpthread
gcc security.c -o ../output/security -lpthread
gcc door.c -o ../output/door -lpthread
gcc motion.c -o ../output/motion -lpthread
gcc keychain.c -o ../output/keychain -lpthread