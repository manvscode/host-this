# Host This
Host This is a tool to shares files with other users on your network. The idea is you can run
the `ht` command for a specified directory and give other users a URL that they can connect to
from their web browser. Then the other users can select which files they would like to download.

![Host This Demo](images/host-this.gif)

## Running
**To run**, execute the `ht` command with the port number and directory:

	
	$ ht -p 9000 .
	  _    _           _      
	 | |  | |         | |     
	 | |__| | ___  ___| |_    
	 |  __  |/ _ \/ __| __|   
	 | |  | | (_) \__ \ |_    
	 |_|  |_|\___/|___/\__|   
		 |__   __| |   (_)    
			| |  | |__  _ ___ 
			| |  | '_ \| / __|
			| |  | | | | \__ \
			|_|  |_| |_|_|___/
	
	Serving files from "." at http://[::1]:9000/

**To share**, tell your friends to go to *http://[::1]:9000/* with their web browser.

**To stop**, just hit `CTRL-C` and you should see:

	
	Stopping server...

## Command Line Options

		-v, --verbose     Toggles verbose mode.
		-a, --address     Sets the address that the web server listens on (default is ::1).
		-p, --port        Sets the port that the web server listens on (default is 8080).
		-t, --title       Sets the title on the web server.

## Build Instructions

### Ubuntu
1. Install dependencies:

	
	apt install -y autoconf automake libtool

2. Build source code by running:

	
	make

### Mac OS X
1. Install [HomeBrew](http://brew.sh/)

	
	/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

2. Install dependencies:

	
	brew install automake autoconf libtool

3. Build source code by running:

	
	make

## Roadmap
* Support https for secure communication.
* Compress the request body with gzip.
* Allow folders to be zipped up and sent over on demand.

## License
	Copyright (C) 2016 by Joseph A. Marrero, http://www.joemarrero.com/
	
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:
	
	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.
	
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
