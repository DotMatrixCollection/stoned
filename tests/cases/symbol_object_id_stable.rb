puts :foo.object_id == :foo.object_id
puts :bar.object_id == :bar.object_id
puts :foo.object_id == :bar.object_id
a = :hello
puts a.object_id == :hello.object_id
