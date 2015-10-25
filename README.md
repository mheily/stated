# stated

stated (prounounced "state dee") is a publish/subscribe mechanism that allows
programs to publish their current state, and notify other interested programs
when the state changes.

Initial development efforts are focused on FreeBSD, but the program should be
easily portable to other Unix-like operating systems.

# Installation 

Run the following to build and install stated:

	make
	sudo make install
	
# Running

To enable stated to start at boot, run the following commands as root(for FreeBSD):

	echo 'stated_enable="YES"' >> /etc/rc.conf
	service stated start
	
# Bugs

stated should be considered beta-quality software, and there are known bugs.

The biggest problem is that the codepaths for publishing "system" state are
incomplete. This means that publishing state for processes running under uid 0
isn't working yet.

# Example usage

Here is a simple example that shows two programs; one acting as the publisher, 
and the other as the subscriber.

	publisher.c
	-----------
	
	state_init();
	state_bind("com.example.hello_world", 0644);
	state_publish("com.example.hello_world", "hi", 2);
	
	subscriber.c
	------------
	
	state_init();
	state_subscribe("com.example.hello_world");
	for (;;) {
		notify_state_t ns;
		int statefd;
		fd_set fds;
		
		statefd = state_get_fd();
		do {
			int result;
			
			FD_ZERO(&fds);
			FD_SET(statefd, &fds);
		    result = select(statefd + 1, &fds, NULL, NULL, NULL);
		} while (result == -1 && errno == EINTR);
		
		if (state_check(&ns, 1) == 1) {
			printf("the new state is: %s\n", ns->ns_state);
		}
	}
