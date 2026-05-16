class Obj
  def set(key, val)
    puts "#{key.inspect} => #{val.inspect}"
  end
end
o = Obj.new
o.set :name, "alice"
o.set :_, 99
o.set :count, 42
