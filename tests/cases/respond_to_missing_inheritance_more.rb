class DynamicBase
  def respond_to_missing?(name, include_private = false)
    name.to_s.start_with?("dyn_") || super
  end

  def method_missing(name, *args)
    if respond_to_missing?(name)
      "handled #{name}"
    else
      super
    end
  end
end

class DynamicChild < DynamicBase
end

obj = DynamicChild.new
p obj.respond_to?(:dyn_name)
p obj.dyn_name
p obj.respond_to?(:missing_name)

begin
  obj.missing_name
rescue NoMethodError => e
  p e.class
end
