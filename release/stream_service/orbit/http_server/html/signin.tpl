<html>
   <head>
    <title>Orbit Signin</title>

    <!-- Bootstrap core CSS -->
    <link href="https://edu.intviu.cn/lib/bootstrap/css/bootstrap.min.css?version=20160706" rel="stylesheet">

    <!-- Custom styles for this template -->
    <link href="/file?file=signin.css" rel="stylesheet">
  </head>

  <body>

    <div class="container">
      <form class="form-signin" action="/signin" method="post">
        <h1 class="form-signin-heading">{{SERVER_NAME}}</h1>
        <h2 class="form-signin-heading">Please sign in</h2>
        <label for="inputEmail" class="sr-only">Email address</label>
        <input type="text" name="inputEmail" class="form-control" placeholder="Email address" required autofocus>
        <label for="inputPassword" class="sr-only">Password</label>
        <input type="password" name="inputPassword" class="form-control" placeholder="Password" required>
        <div class="checkbox">
          <label>
            <input type="checkbox" value="remember-me"> Remember me
          </label>
        </div>
        <button class="btn btn-lg btn-primary btn-block" type="submit">Sign in</button>
      </form>

    </div> <!-- /container -->
  </body>
</html>
