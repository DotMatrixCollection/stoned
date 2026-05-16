puts require("prism")
puts Prism.class
puts Prism::Visitor.class

class PrismWalker < Prism::Visitor
end

puts PrismWalker.superclass == Prism::Visitor
