# object_id, equal?, and identity vs equality
a = "hello"
b = "hello"
c = a

p a == b        # value equality
p a.equal?(b)   # identity: different objects
p a.equal?(c)   # identity: same object
p a.object_id == c.object_id
p a.object_id == b.object_id

# Symbols and small integers have fixed object_ids
p :foo.object_id == :foo.object_id
p 1.object_id == 1.object_id
p true.object_id == true.object_id
p nil.object_id == nil.object_id
p false.object_id == false.object_id

# nil, true, false have unique fixed ids
p nil.object_id
p true.object_id
p false.object_id

# eql? for type-strict equality
p 1.eql?(1)
p 1.eql?(1.0)
p 1 == 1.0

# Hash key lookup uses eql? and hash
p({1 => "int"}[1.0])   # 1 == 1.0 but not eql?, different hash keys
p({1 => "int"}[1])

# Array includes? uses ==
p [1, 2, 3].include?(1.0)
p [1, 2, 3].include?(1)
