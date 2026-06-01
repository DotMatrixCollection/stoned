p ObjectSpace.respond_to?(:each_object)
p ObjectSpace.each_object.to_a
p ObjectSpace.each_object(String).to_a
p GC.respond_to?(:start)
p GC.start
