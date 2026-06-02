class ManualInit
  attr_reader :value

  def initialize(value)
    @value = value
  end

  def ready?
    !@value.nil?
  end
end

obj = ManualInit.allocate
p obj.ready?
p obj.value
obj.send(:initialize, "set")
p obj.ready?
p obj.value
