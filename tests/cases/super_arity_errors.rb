class ATooFew
  def foo(x)
    x
  end
end

class BTooFew < ATooFew
  def foo
    super
  end
end

begin
  p BTooFew.new.foo
rescue => e
  puts e.class
end

class ATooMany
  def foo
    :a
  end
end

class BTooMany < ATooMany
  def foo(x)
    super
  end
end

begin
  p BTooMany.new.foo(1)
rescue => e
  puts e.class
end
