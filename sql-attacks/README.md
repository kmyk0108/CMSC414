# SQL Injections and Web Attacks

This assignment will explore three different web-based attacks: SQL Injection
(SQLi), Cross-Site Scripting (XSS), and Cross-Site Request Forgery (CSRF).

The tasks will all use docker images that have fully-configured
servers that are vulnerable to the various attacks.  Make sure there
is no service on your machine listening on ports 8081, 8082, or
8083. It is unlikely that there is. All docker images are available on
ELMS, and should be installed with `docker image load` in the same way
as for the baseline. **Please note that we will have no access to the
server containers that you are running locally, so you will need to
include *everything* necessary to demonstrate your attacks in the files
you submit.**

For the Java code you will be writing, please ensure that it is
compliant with **Java 1.8**. Make sure that your editor does not
insert "smart" quotes. Single quotes should be ASCII character 0x27,
and double quotes should be ASCII character 0x22. For all tasks,
pay close attention to the formatting specified. There's a difference
between single and double quotes, and the automated grading will
be very unhappy if you use single quotes where you should have used
double quotes. Not following the formatting properly is *the major
reason* for people not getting points on task 2. In particular, the
initial lines for the first 2 tasks represent the fields the
auto-grader will enter into the relevant HTML forms, so please
include **all** of the needed fields **exactly** as you would *enter*
them manually. For tasks 3 and 5, the files you provide will be
used verbatim and in their entirety, so please do not include
additional information. For task 5, you may include HTML comments,
but for task 3, if you feel the need to include additional information,
please place it in a separate file.


# SQL Injection Attacks

Your goal in these tasks is to find ways to exploit the SQL-Injection
vulnerabilities and demonstrate the damage that can be achieved by the attacks.

Steps for running docker image:

1. Start the server by running
   ```
   docker run -d -p 8081:80 --name sql_server sqli
   ```

2. You can access the server in your browser at

    http://localhost:8081/

If you want to examine files in the container, you have several options:

 1. `docker exec -ti sql_server bash`
 2. `docker exec sql_server cat <filename>`
 3. `docker cp sql_server:<filename> <local filename>`

The first option gives you a shell on the running container, though the commands
available to you are somewhat limited. The second will simply dump the contents
of the file (replace `<filename>` with the full path to the file) to STDOUT. The
third will copy the file to a local name, and behaves very similarly to the
normal `cp` command.

For these tasks, the server you are attacking is called Collabtive, which is a
web-based project management system. It has several user accounts configured.
To see all the users' account information, first log in as the admin using the
following password; other users' account information can be obtained from the
post on the front page.

    username: admin 
    password: admin

## Task 1: SQL Injection Attack on SELECT Statements

In this task, you need to manage to log into Collabtive without providing a
password. You can achieve this using SQL injections. Normally, before users
start using Collabtive, they need to login using their user names and
passwords. Collabtive displays a login window to users and ask them to input
their username and password. The authentication is implemented by
`include/class.user.php` in the Collabtive root directory (i.e.,
`/var/www/html/`). It uses the user-provided data to find out whether they match
with the username and user password fields of any record in the database. If
there is a match, it means the user has provided a correct user name and
password combination, and should be allowed to login. Like most web
applications, PHP programs interact with their back-end databases using the
SQL language, in our case MySQL's dialect. In Collabtive, the following SQL
query is constructed in class.user.php to authenticate users:

    $sel1 = mysql_query ("SELECT ID, name, locale, lastlogin, gender, 
                          FROM USERS_TABLE 
                          WHERE (name = '$user' OR email = '$user') AND pass = '$pass'");
    
    $chk = mysql_fetch_array($sel1);
    
    if (found one record) 
        then {allow the user to login}

In the above SQL statement, the `USERS TABLE` is a macro in PHP, and will be
replaced by the users table named user. The variable `$user` holds the string
typed in the Username textbox, and `$pass` holds the string typed in the
Password textbox. User's inputs in these two textboxs are placed directly in
the SQL query string.

### Submission

Your task is to login as bob without using his password. Please
submit a file called `task1.txt`. The first two lines should be the values you
entered in the login and password fields surrounded by double quotes. The two
lines should look exactly as follows, with your inputs in place of the `-`

    login="-" 
    password="-" 

Follow this with a short explanation of what your input causes to happen.

## Task 2: SQL Injection on UPDATE Statements

In this task, you need to make an unauthorized modification to the database.
Your goal is to modify another user's profile using SQL injections. In
Collabtive, if users want to update their profiles, they can go to "My
account", click the "Edit" link, and then fill out a form to update the
profile information. After the user sends the update request to the server, an
`UPDATE SQL` statement will be constructed in `include/class.user.php`. The
objective of this statement is to modify the current user's profile
information in the users table. There is a SQL injection vulnerability in this
SQL statement. Please find the vulnerability, and then use it to change
another user's profile without knowing their password. For example, if you are
logged in as Alice, your goal is to use the vulnerability to modify Bob's
profile information, including Bob's password. After the attack, you should be
able to log into Bob's account.

### Submission

Your task is to change Bob's password by modifying a different
users profile. Please submit a file called `task2.txt` with the following
format:

 * line 1: Bob's new password after your injection, wrapped in double quotes.

 * line 2: User whose profile you are modifying (*not* Bob or Admin), without
           quotes.

 * lines 3-n:
     One line for each profile parameter you changed, like so:

     ```
     "param"="new value"
     ```

     Please note that both the parameter name and new value should be
     wrapped in double quotes, so that we can clearly see any spaces
     you might have added. Also, the parameter name should be exactly
     as it appears in the HTML source code, *not* what you see on the
     webpage. You will have to examine the HTML or network traffic for
     these. (Hint: If your parameters begin with capital letters or
     dollar signs, you have the wrong parameter names.) The new value
     should be *exactly* what you type into the corresponding field of
     the form, including the injection.  If you don't do this, the
     automated grading will mark this task as failing. *All*
     parameters that you need to set must be included here for the
     automated grading to succeed.

 * line n+1: A blank line 

 * lines n+2 to end: The remaining lines should contain a short write
     up explaining the steps you used to create a working SQL
     injection attack that updates Bob's password to a new value.
     This is especially important if you mis-format the parameter
     lines, since it's the most likely way we'll be able to figure out
     what you actually should have entered during manual regrading, if
     needed.

For example (indentation not required):

    "newpasswd"
    alice
    "foo"="bar"
    "baz"="blah"

    Other explanatory stuff.

For the SQL Injection attacks, your repository should include:

 * task1.txt 
 * task2.txt 


# XSS Attack

Your goal in these tasks is to find ways to exploit the Cross-Site Scripting
vulnerabilities and demonstrate the damage that can be achieved by the attacks.

Steps for running docker image:

1. Start the server by running

   ```
   docker run -d -p 8082:80 --name xss_server xss
   ```
   
2. You can access the server in your browser at

    http://localhost:8082/

For these tasks, the server you are attacking is called Elgg, which is a social
networking application. It has several user accounts configured, with the
following credentials:

    USER     USERNAME  PASSWORD
    Admin    admin     seedelgg
    Alice    alice     seedalice
    Boby     boby      seedboby
    Charlie  charlie   seedcharlie
    Samy     samy      seedsamy

## Warmup - No submission Necessary 

The objective of this task is to embed a JavaScript program in your
Elgg profile, such that when another user views your profile, the
JavaScript program will be executed and an alert window will be
displayed. The following JavaScript program will display an alert
window:
 	
      <script>alert('XSS')</script> 

If you embed the above JavaScript code in your profile (e.g. in the
brief description field), then any user who views your profile will
see the alert window.
  
Our next objective is to embed a JavaScript program in your Elgg
profile, such that when another user views your profile, the user's
cookies will be displayed in the alert window. This can be done by
adding some additional code to the JavaScript program in the previous
example:
  
      <script>alert(document.cookie);</script> 

## Task 3: Stealing Cookies from the Victim's machine 

In the previous task, the malicious JavaScript code written by the
attacker can print out the user's cookies, but only the user can see
the cookies, not the attacker. In this task, the attacker wants the
JavaScript code to send the cookies to themselves. To achieve this,
the malicious JavaScript code needs to send an HTTP request to the
attacker, with the cookies appended to the request.
 
We can do this by having the malicious JavaScript insert an `<img>`
tag with `src` attribute set to the attacker's machine. When the
JavaScript inserts the `img` tag, the browser tries to load the image
from the URL in the `src` field; this results in an HTTP GET request
sent to the attacker's machine. Your JavaScript code should send the
cookies to the port 5555 of the attacker's machine, where the attacker
has a TCP server listening to the same port. The server can print out
whatever it receives. The TCP server program is included as
`echoserv.py`, which is a python 3 script, and can be run directly on
your host (that is, not in a container).
 
**Note:** Some popular software, like Android Studio, uses port
5555. If you're having issues with not being able to start the echo
server, try changing 5555 to 5556 (or some other number) everywhere
you use it, including the `PORT` variable in the script. Keep in mind
that you'll need to make sure `task3.txt` is using 5555 when you're
done, since that's how we're testing your submission.

 
### Submission

Please submit a file called `task3.txt`. The grading will be done as
follows:

1. We will run the echo server on localhost on port 5555.
2. We will edit the brief description field acting as the user Alice
   using the message contents as specified by your `task3.txt`. We
   will use the *entire* contents of this file, so if you need to
   provide additional details, please do so in a separate file.
3. Then, when Boby opens this message, Boby's cookie should be printed
   by the echo server. If you had to use a different port, don't
   forget to change it back to 5555 once it's working!

## Task 4: Session Hijacking using the Stolen Cookies   

After stealing the victim's cookies, the attacker can do whatever the
victim can do to the Elgg web server, including adding and deleting
friends on behalf of the victim, deleting the victim's posts,
etc. Essentially, the attacker has hijacked the victim's session. In
this task, we will launch this session hijacking attack, and write a
program to add a friend on behalf of the victim.
 
To add a friend for the victim, we should first find out how a
legitimate user adds a friend in Elgg. More specifically, we need to
figure out what are sent to the server when a user adds a friend. A
screenshot of the request is provided to you. From the contents, you
can identify all the parameters in the request.
 
Once we have understood what the HTTP request for adding friends look
like, we can write a Java program to send out the same HTTP
request. The Elgg server cannot distinguish whether the request is
sent out by the user's browser or by the attacker's Java program. As
long as we set all the parameters correctly, and the session cookie is
attached, the server will accept and process the project-posting HTTP
request. To simplify your task, we provide you with a sample Java
program that does the following:

1. Open a connection to web server.  
2. Set the necessary HTTP header information.  
3. Send the request to web server.  
4. Get the response from web server. 
 
**Note1:** Elgg uses two parameters `__elgg_ts` and
`__elgg_token`. Make sure that you set these parameters correctly for
your attack to succeed.  Also, please note down the correct guid of
the friend who needs to be added to the friend list.  You need to use
that guid in the program code for the attack to succeed.

**Note2:** You can compile a java program into bytecode for **Java
1.8** by running

    javac --release=8 HTTPSimpleForge.java

on the console. You can then run the bytecode by running

    java HTTPSimpleForge  
 
### Submission

Please submit a file called `HTTPSimpleForge.java`.

### Grading

Your java file will be compiled into byte code and executed. We will
login as Alice, and try to add Boby as a friend to Alice by running
your compiled code. Make sure you include the correct guid and
parameters in the code for the above scenario to execute properly.
 
**Input:** Your java program should read from an input file called
`HTTPSimpleForge.txt`. This filename should be hard-coded into your
program; it *will not* be passed on the command line. The first line
of the input file contains the `__elgg_ts` token (absolute value), the
second line contains the `__elgg_token` (absolute value) and the third
line would contain the Cookie HTTP header value:

    Elgg=<<cookie value>>

As an example:

    1402467511
    80923e114f5d6c5606b7efaa389213b3
    Elgg=7pgvml3vh04m9k99qj5r7ceho4

You can create a text file locally for testing out your code, however,
you do not need to submit this file as part of your submission. We
will use our own `HTTPSimpleForge.txt` file for checking.


# CSRF Attack

Your goal in these tasks is to find ways to exploit a Cross-Site Request Forgery
vulnerability.

Steps for running docker image:

1. Start the server by running

   ```
   docker run -d -p 8083:80 -v "$(pwd):/var/www/CSRF/Attacker" --name csrf_server csrf
   ```

2. You can access the server in your browser at

    http://localhost:8083/

This is running a slightly different version of Elgg, but has the same users
and credentials.
 
## Task 5: CSRF Attack using GET Request 
 
In this task, we need two people in the Elgg social network: Alice and
Boby.  Alice wants to become a friend to Boby, but Boby refuses to add
Alice to his Elgg friend list. Alice decides to use the CSRF attack to
achieve her goal. She sends Boby a URL (via an email or a posting in
Elgg); Boby, curious about it, and clicks on the URL. Pretend that you
are Alice, and think about how you can construct the contents of the
web page, so as soon as Boby visits the web page, Alice is added to
the friend list of Boby (assuming Boby has an active session with
Elgg).

To add a friend to the victim, we need to identify the Add Friend HTTP
request, which is a GET request. In this task, you are not allowed to
write JavaScript code to launch the CSRF attack. Your job is to make
the attack successful as soon as Boby visits the web page, without
even clicking on the page (hint: you can use the `img` tag, which
automatically triggers an `HTTP GET` request).

Whenever the victim user visits the crafted web page in the malicious
site, the web browser automatically issues an `HTTP GET` request for
the URL contained in the `img` tag. Because the web browser
automatically attaches the session cookie to the request, the trusted
site cannot distinguish the malicious request from the genuine request
and ends up processing the request, compromising the victim user's
session integrity.

Observe the request structure for adding a new friend and then use
this to forge a new request to the application. When the victim user
visits the malicious web page, a malicious request for adding a friend
should be injected into the victim's active session with Elgg.
 
### Submission

You are required to submit a file named `task5.html`. When a victim
user named Boby is logged in, and visits the attacker website
`localhost:<<port>>/task5.html` in another tab, Alice should be added
as a friend to Boby's Friend List.
 
To test this, you will need to place the `task5.html` file under the directory
`/var/www/CSRF/elgg/` in the docker container. You can do this by running
  
    docker cp task5.html csrf_server:/var/www/CSRF/elgg

**Tip:** Your browser may not refresh on its own. You might need to
press the reload/refresh button to reload the page, to see if Alice is
added as a friend to Boby's account.
