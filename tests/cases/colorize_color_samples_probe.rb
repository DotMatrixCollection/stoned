class ColorizedString < String
end

class String
  def self.color_samples
    %w[black red green].permutation(2).map do |colors|
      ColorizedString.new(colors.join("+"))
    end
  end
end

samples = String.color_samples
puts samples.length
puts samples.map(&:class).uniq.inspect
puts samples.map(&:to_s).join(",")
puts samples.first.upcase
