class AllocBox
end

obj = AllocBox.allocate
puts obj.class
p obj.is_a?(AllocBox)
