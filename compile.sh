clear
echo "starting compilation..."
g++ -std=c++11 src/main.cpp lib/json/jsoncpp.cpp -o main -Ilib -lcurl -lpthread
echo "compilation finished..."

