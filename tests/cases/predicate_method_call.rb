class Foo
  def ready?; true; end
  def go!; "going"; end
  def test
    puts ready?
    puts go!
  end
end
Foo.new.test
