class UsingProbe
  using Module.new {
    refine String do
      def loud
        upcase
      end
    end
  }
end

puts "ok"
