class Integer
  def factorial
    return 1 if self <= 1
    self * (self - 1).factorial
  end

  def prime?
    return false if self < 2
    (2..Math.sqrt(self).to_i).none? { |i| self % i == 0 }
  end
end

p 5.factorial
p 10.factorial
p 7.prime?
p 10.prime?
p (1..20).select(&:prime?)
