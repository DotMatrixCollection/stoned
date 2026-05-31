class Seq
  include Enumerable

  def each
    return to_enum(:each) unless block_given?
    yield 1
    yield 2
    yield 3
  end
end

seq = Seq.new

p [
  seq.find,
  seq.detect,
  seq.map,
  seq.collect,
  seq.group_by
].map { |e| e.class }

p seq.find.to_a
p seq.map.to_a
p seq.group_by.to_a
