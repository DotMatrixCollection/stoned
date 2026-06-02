# Comparable mixin enables sort, min, max, min_by, max_by
class Product
  include Comparable
  attr_accessor :name, :price, :rating

  def initialize(name, price, rating)
    @name, @price, @rating = name, price, rating
  end

  def <=>(other)
    price <=> other.price
  end

  def to_s = "#{name}($#{price})"
end

items = [
  Product.new("Widget", 9.99, 4),
  Product.new("Gadget", 4.99, 5),
  Product.new("Doohickey", 14.99, 3),
  Product.new("Thingamajig", 7.49, 4),
]

p items.min.name
p items.max.name
p items.sort.map(&:name)
p items.minmax.map(&:name)

# sort_by with different key
p items.sort_by(&:rating).map(&:name)
p items.min_by(&:rating).name
p items.max_by(&:rating).name

# between? uses <=>
widget = items[0]
gadget = items[1]
doohickey = items[2]
p widget.between?(gadget, doohickey)
p gadget.between?(widget, doohickey)

# clamp
p widget.clamp(gadget, doohickey).name
