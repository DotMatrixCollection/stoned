# caller returns array of strings describing the call stack
def inner
  caller
end

def outer
  inner
end

stack = outer
p stack.is_a?(Array)
p stack.first.is_a?(String)
p stack.first.include?("caller_basic")

# caller(n) returns n frames
def get_caller_1
  caller(0, 1)
end
frames = get_caller_1
p frames.length
p frames.first.is_a?(String)

# caller_locations returns Thread::Backtrace::Location objects
def get_locations
  caller_locations
end

locs = get_locations
p locs.is_a?(Array)
p locs.first.is_a?(Thread::Backtrace::Location)
p locs.first.lineno.is_a?(Integer)
p locs.first.lineno > 0
p locs.first.label.is_a?(String)
p locs.first.path.is_a?(String)

# caller_locations(0) includes current frame
def self_frame
  caller_locations(0, 1).first.label
end
p self_frame
