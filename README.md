CS4061_project_3

Names:

Jacob Sprafka (spraf013@umn.edu) and Christopher Schumacher (schum865@umn.edu)

Test Machine:

csel-kh1260-03.cselabs.umn.edu

Date:

4/18/2022

Purpose of this Program:

The purpose of this program is to function like a web server which retreives requests for the user. The user will connect to the webserver and request a file, the server will look for the file under it's root directory. If it finds the file it will return it to the user. This program is multi-threaded, dispatcher threads will accept incoming connections, read the associated requests, and store them in a buffer. Worker threads will remove the requests from the buffer and service them. This server can handle requests of any size in the form: HTML, GIF, JPEG, TXT. Use cntrl-C to stop the server when finished.

How to compile and run:

extract testing.zip

make clean

make

Start server as: 

./web_server port# path num_dispatch num_workers dynamic_flag cache_flag queue_length cache_length

-> We did not implement extra credit options: Cache and Dynamic thread pool size. Therefore- dynamic_flag = 0, cache_flag = 0, chache_length > 0.

Example (terminal open in project directory)- 

./web_server 2000 ./testing 5 5 0 0 50 1

Open another terminal and connect to the server with a request.

Example- 

wget http://127.0.0.1:2000/image/jpg/29.jpg

Contributions of each team member:

Intermediate Submission

TerminationHandler - Jacob

Dispatcher - Jacob

Final Submission

Dispatcher - Jacob / Chris 

Worker - Jacob / Chris

README.md - Chris
