module MatrixA
  def call
    "A>#{super}"
  end
end

module MatrixB
  include MatrixA

  def call
    "B>#{super}"
  end
end

module MatrixPrepended
  def call
    "P>#{super}"
  end
end

class MatrixBase
  def call
    "base"
  end

  protected

  def family
    "family"
  end

  private

  def hidden
    "hidden"
  end
end

class MatrixChild < MatrixBase
  include MatrixB
  prepend MatrixPrepended

  def call
    "child>#{super}"
  end

  def check(other)
    "#{other.family}:#{hidden}"
  end
end

obj = MatrixChild.new

puts MatrixChild.ancestors.first(5).map(&:name).inspect
puts obj.call
puts obj.check(MatrixChild.new)
puts obj.respond_to?(:family)
puts obj.respond_to?(:family, true)
puts obj.respond_to?(:hidden)
puts obj.respond_to?(:hidden, true)

begin
  obj.public_send(:family)
rescue => e
  puts e.class
end

class MatrixDynamic
  def method_missing(name, *args)
    return "dynamic" if name == :dynamic_value
    super
  end

  private

  def respond_to_missing?(name, include_private = false)
    name == :dynamic_value || super
  end
end

dyn = MatrixDynamic.new
puts dyn.dynamic_value
puts dyn.respond_to?(:dynamic_value)
puts dyn.public_methods.include?(:respond_to_missing?)

module MatrixMethodPrepended
  def value
    "prepended>#{super}"
  end
end

class MatrixMethodBase
  def value
    "base"
  end
end

class MatrixMethodChild < MatrixMethodBase
  prepend MatrixMethodPrepended

  def value
    "child>#{super}"
  end
end

method = MatrixMethodChild.new.method(:value)
puts method.owner.name
puts method.call
puts method.super_method.owner.name
puts method.super_method.call
puts method.super_method.super_method.owner.name
puts method.super_method.super_method.call

module MatrixClassSideA
  def build
    "A"
  end

  def chain
    "A"
  end
end

class MatrixClassSideBefore
  def self.build
    "before"
  end

  extend MatrixClassSideA
end

class MatrixClassSideAfter
  extend MatrixClassSideA

  def self.build
    "after"
  end
end

class MatrixClassSideOnly
  extend MatrixClassSideA
end

class MatrixClassSideSuper
  extend MatrixClassSideA

  def self.chain
    "class>#{super}"
  end
end

puts MatrixClassSideBefore.build
puts MatrixClassSideAfter.build
puts MatrixClassSideOnly.build
puts MatrixClassSideSuper.chain
puts MatrixClassSideBefore.respond_to?(:build)
puts MatrixClassSideBefore.methods.include?(:build)
