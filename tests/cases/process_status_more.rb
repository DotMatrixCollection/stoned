p Process.pid > 0
p Process::Status.new.class rescue p $?.class
