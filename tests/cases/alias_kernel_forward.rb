module MyLoader
  alias my_require require
  def load_it(f)
    my_require f rescue nil
  end
end
puts "ok"
