# runonce


    USAGE: runonce -c atom -e 'atom foo.txt' -s 'atom foo.txt'
        -s server command
        -c wm class
        -e client command

    if class not provided, deduce class by provided command
    if window matching class exists:
        switch to window
    else:
        if server command is provided:
            execute server command
        if client command is provided:
            execute client command

    if only class is provided, just switch to window
    if only client command is provided, switch and execute
    if  nly server command is provided, switch or execute
    if both client and server provided, {switch and execute client} or execute server 
