local fork = require("fork")

fork.fork()

fork.write1(fork.read())

fork.write("main end.\r\n")

