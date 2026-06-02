class Incomparable
  def <=>(other) = nil
end

begin
  [Incomparable.new, Incomparable.new].sort
rescue ArgumentError => e
  p e.class.to_s
end

begin
  [1, nil, 2].sort
rescue ArgumentError => e
  p e.class.to_s
end

p [1, 2, 3].sort { |a, b| a <=> b }
