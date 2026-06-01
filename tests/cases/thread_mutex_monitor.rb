require "thread"
require "monitor"

mutex = Thread::Mutex.new
p mutex.locked?
p mutex.synchronize { "ok" }
p mutex.locked?

monitor = Monitor.new
p monitor.synchronize { 12 }
