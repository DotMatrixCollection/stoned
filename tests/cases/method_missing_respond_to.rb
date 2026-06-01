class DynProxy
  def method_missing(name, *args)
    if name.to_s.start_with?("find_by_")
      attr = name.to_s.sub("find_by_", "")
      "found #{attr}=#{args.first}"
    else
      super
    end
  end

  def respond_to_missing?(name, include_private = false)
    name.to_s.start_with?("find_by_") || super
  end
end

d = DynProxy.new
p d.find_by_name("Alice")
p d.find_by_age(30)
p d.respond_to?(:find_by_name)
p d.respond_to?(:nonexistent)

begin
  d.something_else
rescue NoMethodError => e
  p e.message
end
