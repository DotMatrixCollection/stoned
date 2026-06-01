class AttrMore
  attr_accessor :name
end

o = AttrMore.new
o.name = "Ada"
p o.name
p AttrMore.instance_methods.include?(:name)
