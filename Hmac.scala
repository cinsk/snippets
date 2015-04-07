// You may need to add following in your build.sbt
//
// libraryDependencies += "commons-codec" % "commons-codec" % "1.9"
//
object Hmac {
  //import java.io._
  //import java.security._
  //import java.security.spec._
  import javax.crypto.spec._
  import javax.crypto._
  import org.apache.commons.codec.binary.Base64

  val HMAC_SHA1_ALGORITHM = "HmacSHA1"

  def inBase64(secret: String, data: String) = {
    val key = new SecretKeySpec(secret.getBytes(), HMAC_SHA1_ALGORITHM);
    val mac = Mac.getInstance(HMAC_SHA1_ALGORITHM);
    mac.init(key)
    val rawHmac = mac.doFinal(data.getBytes())

    val b64 = new String(Base64.encodeBase64(rawHmac))

    b64
  }

  // We cannot use Base64.encodeBase64(data, chunked, urlsafe) since
  // it will not add any padding when using the URL-safe alphabet.
  // Instead, we will try to build URL-safe string by our own, as
  // described in
  // https://docs.python.org/2/library/base64.html#base64.urlsafe%5Fb64encode
  def inBase64UrlSafe(secret: String, data: String) =
    inBase64(secret, data).map(_ match {
                                 case '+' => '-'
                                 case '/' => '_'
                                 case c@_ => c
                               })
}
